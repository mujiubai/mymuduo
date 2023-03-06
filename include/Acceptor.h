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
  //开始监听
  void listen();

 private:
  // listenfd有事件发生即有新用户连接，调用此函数
  void handleRead();

  EventLoop *loop_;  // acceptor使用的用户定义的loop，也叫mainloop
  Socket acceptSocket_;                          // listenfd对应的socket
  Channel acceptChannel_;                        // listenfd对应的channel
  NewConnectionCallback newConnectionCallback_;  //当有新连接到来，需要做的回调
  bool listenning_;                              //是否监听标志
};
}  // namespace muduo
