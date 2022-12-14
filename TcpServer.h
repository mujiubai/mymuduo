#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <unordered_map>

#include "Acceptor.h"
#include "Buffer.h"
#include "Callbacks.h"
#include "EventLoop.h"
#include "EventLoopThreadPool.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include "noncopyable.h"

namespace muduo {

/**
 * @brief用户使用TcpServer编写服务器程序
 *
 */
class TcpServer : noncopyable {
 public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;

  enum Option {
    kNoReusePort,
    kReusePort,
  };

  TcpServer(EventLoop *loop, const InetAddress &listenAddr,
            const std::string nameArg, Option option = kNoReusePort);
  ~TcpServer();

  void setThreadInitCallback(const ThreadInitCallback &cb) {
    threadInitCallback_ = cb;
  }
  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }
  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }

  //设置低层subloop的个数
  void setThreadNum(int numThreads);

  //开启服务器监听
  void start();

 private:
  //当Acceptor有新连接时会调用的回调函数
  void newConnection(int sockfd, const InetAddress &peerAddr);
  
  void removeConnection(const TcpConnectionPtr &conn);
  void removeConnectionInLoop(const TcpConnectionPtr &conn);

  using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

  EventLoop *loop_;  //用户定义的loop
  const std::string ipPort_;
  const std::string name_;
  std::unique_ptr<Acceptor> acceptor_;  // mainLoop的监听新连接事件
  std::shared_ptr<EventLoopThreadPool> threadPool_;  // one loop per thread

  ConnectionCallback connectionCallback_;        //有新连接时的回调
  MessageCallback messageCallback_;              //有读写消息的回调
  WriteCompleteCallback writeCompleteCallback_;  //消息发送完成的回调

  ThreadInitCallback threadInitCallback_;  //线程初始化的回调
  std::atomic_int started_;

  int nextConnId_;  // TcpConnection的id，是用来加到TcpConnection名字中
  ConnectionMap connections_;  //保存所有的连接
};
}  // namespace muduo