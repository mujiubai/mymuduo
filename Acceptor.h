#pragma once

#include <functional>

#include "Channel.h"
#include "Socket.h"
#include "noncopyable.h"

namespace muduo {

class EventLoop;
class InetAddress;

class Acceptor : noncopyable {
 public:
  using NewConnectionCallback =
      std::function<void(int sockfd, const InetAddress &)>;

  Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback &cb) {
    newConnectionCallback_ = cb;
  }

  bool listenning() const { return listenning_; }
  void listen();

 private:
  void handleRead();

  EventLoop *loop_;  // acceptor使用的用户定义的loop，也叫mainloop
  Socket acceptSocket_;
  Channel acceptChannel_;
  NewConnectionCallback newConnectionCallback_;  //当有新连接到来，需要做的事
  bool listenning_;
};
}  // namespace muduo
