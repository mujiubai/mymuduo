#include "Acceptor.h"
#include "InetAddress.h"

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "Logger.h"

using namespace muduo;

static int createNonblocking() {
  int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC,
                        0);
  if (sockfd < 0) {
    LOG_FATAL("%s:%s:%d listen socket create err:%d \n", __FILE__, __FUNCTION__,
              __LINE__, errno);
  }
  return sockfd;
}

Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr,
                   bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()),
      acceptChannel_(loop, acceptSocket_.fd()),
      listenning_(false) {
  acceptSocket_.setReuseAddr(true);
  acceptSocket_.setReusePort(reuseport);
  acceptSocket_.bindAddress(listenAddr);  // bind 绑定
  //如果有新用户连接，要执行一个回调(connfd打包成channel，再唤醒channel给到的subloop)
  acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor() {
  acceptChannel_.disableAll();
  acceptChannel_.remove();
}

void Acceptor::listen() {
  listenning_ = true;
  acceptSocket_
      .listen();  //不懂这里为啥还要设置监听，不是要在下面channel中设置监听吗
  acceptChannel_.enableReading();
}

// listenfd有事件发生即有新用户连接，调用此函数
void Acceptor::handleRead() {
  InetAddress peerAddr;
  int connfd = acceptSocket_.accept(&peerAddr);
  if (connfd >= 0) {
    if (newConnectionCallback_) {
      //轮询找到subloop，将其唤醒，分发当前客户端的channel
      newConnectionCallback_(connfd, peerAddr);
    } else {
      ::close(connfd);
    }
  } else {
    LOG_ERROR("%s:%s:%d accept err:%d \n", __FILE__, __FUNCTION__, __LINE__,
              errno);
    // EMFILE表示服务器sockfd资源用完
    if (errno == EMFILE) {
      LOG_ERROR("%s:%s:%d sockfd reached limit \n", __FILE__, __FUNCTION__,
                __LINE__);
    }
  }
}