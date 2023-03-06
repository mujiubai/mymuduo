#pragma once

#include "noncopyable.h"

namespace muduo {

class InetAddress;

class Socket : noncopyable {
 public:
  explicit Socket(int sockfd) : sockfd_(sockfd) {}
  ~Socket();

  int fd() const { return sockfd_; }
  void bindAddress(const InetAddress &localAddr);
  void listen();
  int accept(InetAddress *peeraddr);

  void shutdownWrite();

  //设置不等待而直接发送
  void setTcpNoDelay(bool on);
  //设置端口释放后立即就可以被再次使用
  void setReuseAddr(bool on);
  //允许许多个线程或进程，绑定在同一个端口上
  void setReusePort(bool on);
  //设置是否开启心跳，2小时一次
  void setKeepAlive(bool on);

 private:
  const int sockfd_;
};
}  // namespace muduo