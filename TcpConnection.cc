#include "TcpConnection.h"

#include <errno.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Socket.h"

using namespace muduo;

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
  if (loop == nullptr) {
    LOG_FATAL("%s:%s:%d TcpConnection loop is nullptr!", __FILE__, __FUNCTION__,
              __LINE__);
  }
  return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, const std::string &nameArg,
                             int sockfd, const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop)),
      name_(nameArg),
      state_(kConnecting),
      reading_(true),
      socket_(new Socket(sockfd)),
      channel_(new Channel(loop, sockfd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024) {
  //设置channel的各种回调
  // poller给channel通知感兴趣的事件发生后，channel会回调相应的函数
  channel_->setReadCallback(
      bind(&TcpConnection::handleRead, this, std::placeholders::_1));
  channel_->setWriteCallback(bind(&TcpConnection::handleWrite, this));
  channel_->setCloseCallback(bind(&TcpConnection::handleClose, this));
  channel_->setErrorCallback(bind(&TcpConnection::handleError, this));

  LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n", name_.c_str(), sockfd);
  socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
  LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d", name_.c_str(),
           channel_->fd(), (int)state_);
}

void TcpConnection::handleRead(Timestamp receiveTime) {
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0) {
    //已建立连接的用户，有可读事件发生了，调用用户传入的回调操作
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  } else if (n == 0) {
    handleClose();
  } else {
    errno = savedErrno;
    LOG_ERROR("TcpConnection::handleRead error\n");
    handleError();
  }
}

//啥时候执行写回调？poller怎么通知写回调，数据不是都先到buffer中的吗
void TcpConnection::handleWrite() {
  if (channel_->isWriting()) {
    int savedError = 0;
    ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedError);
    if (n > 0) {
      outputBuffer_.retrieve(n);
      if (outputBuffer_.readableBytes() == 0) {
        channel_->disableWriting();
        if (writeCompleteCallback_) {
          //唤醒loop_对应的thread线程，执行回调
          //其实此时就是在loop_对应的thread线程中
          loop_->queueInLoop(
              std::bind(writeCompleteCallback_, shared_from_this()));
        }
        //当用户shutdown时数据如果没发送完成，不会真的shutdown，只是将标志置为kDisconnecting，而是会等待用户把数据发送完成
        //当数据发送完成后，则再执行一次shutdownInLoop
        if (state_ == kDisconnecting) {
          shutdownInLoop();
        }
      }
    } else {
      LOG_ERROR("TcpConnection::handleWrite() error\n");
    }
  } else {
    LOG_ERROR("TcpConnection fd=%d is down, no more writing \n",
              channel_->fd());
  }
}

//发生关闭事件后，调用的回调
void TcpConnection::handleClose() {
  LOG_INFO("fd=%d state=%d \n", channel_->fd(), (int)state_);
  setState(kDisconnected);
  channel_->disableAll();

  TcpConnectionPtr connPtr(shared_from_this());
  //有点没搞明白为啥还要使用连接的回调，直接调用关闭回调不就好了
  connectionCallback_(connPtr);  //执行连接关闭回调
  //必须在最后一行
  closeCallback_(connPtr);  //关闭连接回调
}

//发生错误事件后，打印出错误信息
void TcpConnection::handleError() {
  int optval;
  socklen_t optlen = sizeof optval;
  int err = 0;
  // getsockopt()函数用于获取任意类型、任意状态套接口的选项当前值，并把结果存入optval
  if (::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) <
      0) {  //小于0，则获取失败
    err = errno;
  } else {
    err = optval;
  }
  LOG_ERROR("TcpConnection::handleError() name:%s -SO_ERROR:%d \n ",
            name_.c_str(), err);
}

void TcpConnection::send(const std::string &buf) {
  if (state_ == kConnected) {
    if (loop_->isInLoopThread()) {
      sendInLoop(buf.c_str(), buf.size());
    } else {
      loop_->runInLoop(
          std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
    }
  }
}

//发送数据
//应用写得快，而内核发送数据慢，需要把待发送数据写入缓冲区，而且设置水位回调
void TcpConnection::sendInLoop(const void *message, size_t len) {
  size_t nwrote = 0;
  size_t remaining = len;
  bool faultError = false;

  //之前已经调用过shutdown，不能再进行发送了
  if (state_ == kDisconnected) {
    LOG_ERROR("disconnected, give up writing");
    return;
  }

  // channel第一次开始写数据且缓冲区没有待发送数据
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0) {
    nwrote = ::write(channel_->fd(), message, len);
    if (nwrote >= 0) {
      remaining = len - nwrote;
      if (remaining == 0 && writeCompleteCallback_) {
        //当一次将数据发送完成，就不用再给channel设置epollout事件了
        loop_->queueInLoop(
            std::bind(writeCompleteCallback_, shared_from_this()));
      }
    } else {
      nwrote = 0;
      if (errno != EWOULDBLOCK) {
        LOG_ERROR("TcpConnection::sendInLoop\n");
        if (errno == EPIPE || errno == ECONNREFUSED) {
          faultError = true;
        }
      }
    }
  }

  /****************************************************/
  //说明这次write没有全部发送出去，剩余数据需要保存到缓冲区中，
  //然后给channel注册epollout事件，poller发现tcp的发送缓冲区有空间，会通知相应的sock-channel，调用WriteCallback方法
  //最终也就是调用handleWrite方法，把发送缓冲区中的数据全部发送完成
  if (!faultError && remaining > 0) {
    //目前发送缓冲区剩余的待发送数据长度
    size_t oldLen = outputBuffer_.readableBytes();
    if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ &&
        highWaterMarkCallback_) {
      loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(),
                                   oldLen + remaining));
    }
    outputBuffer_.append((char *)message + nwrote, remaining);
    if (!channel_->isWriting()) {
      //这里一定要注册channel的写事件，否则poller不会给channel通知epollout
      channel_->enableReading();
    }
  }
}

//连接建立回调  还不知道这个函数在哪调用
void TcpConnection::connectEstablished() {
  setState(kConnecting);
  //将channel与此对象绑定，使得channel能观察此对象是否存活，避免调用回调时出错
  channel_->tie(shared_from_this());
  //向poller注册channel的epollin事件
  channel_->enableReading();

  //新连接建立，执行回调
  connectionCallback_(shared_from_this());
}

//连接销毁回调 还不知道这个函数在哪调用
void TcpConnection::connectDestroyed() {
  if (state_ == kConnected) {
    setState(kDisconnected);
    //把channel所有感兴趣事件从poller中删除
    channel_->disableAll();
    connectionCallback_(shared_from_this());
  }
  //将channel从poller中删除
  channel_->remove();
}

//关闭连接
void TcpConnection::shutdown() {
  if (state_ == kConnected) {
    setState(kDisconnecting);
    loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
  }
}
void TcpConnection::shutdownInLoop() {
  //当用户shutdown时数据如果没发送完成，不会真的shutdown，只是将标志置为kDisconnecting，而是会等待用户把数据发送完成
  //当缓冲区发送完数据后，会判断再执行一次shutdownInLoop函数
  if (!channel_->isWriting()) {  //说明当前outputbuffer已经全部发送完成
    //poller会检测到关闭事件，通知channel回调关闭回调函数TcpConnection::handleClose
    socket_->shutdownWrite();
  }
}