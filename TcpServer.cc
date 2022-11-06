#include "TcpServer.h"

#include <functional>

#include "Logger.h"

using namespace muduo;

static EventLoop *CheckLoopNotNull(EventLoop *loop) {
  if (loop == nullptr) {
    LOG_FATAL("%s:%s:%d mainLoop is nullptr!", __FILE__, __FUNCTION__,
              __LINE__);
  }
  return loop;
}

TcpServer::TcpServer(EventLoop *loop, const InetAddress &listenAddr,
                     const std::string nameArg, Option option)
    : loop_(CheckLoopNotNull(loop)),
      ipPort_(listenAddr.toIpPort()),
      name_(nameArg),
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
      threadPool_(new EventLoopThreadPool(loop, name_)),
      connectionCallback_(),
      messageCallback_(),
      nextConnId_(1) {
  //有新用户连接时，执行newConnection函数回调
  acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this,
                                                std::placeholders::_1,
                                                std::placeholders::_2));
}

TcpServer::~TcpServer() {}

//设置低层subloop的个数
void TcpServer::setThreadNum(int numThreads) {
  threadPool_->setThreadNum(numThreads);
}

//开启服务器监听
void TcpServer::start() {
  //防止一个TcpServer对象被start多次
  if (started_++ == 0) {
    threadPool_->start(threadInitCallback_);
    loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
  }
}

//根据轮询算法选择一个subLoop，唤醒subLoop，把当前connfd封装成channel分发给subloop
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr) {
  EventLoop *ioLoop = threadPool_->getNextLoop();
}
void TcpServer::removeConnection(const TcpConnection &conn) {}
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn) {}