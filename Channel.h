#pragma once

#include <functional>
#include <memory>

#include "Timestamp.h"
#include "noncopyable.h"

namespace muduo {

class EventLoop;

// Channel 理解为消息通道 封装了sockfd和其感兴趣的事件和返回的事件
class Channel : noncopyable {
 public:
  using EventCallback = std::function<void()>;
  using ReadEventCallback = std::function<void(Timestamp)>;
  Channel(EventLoop *loop, int fd);
  ~Channel();
  void HandleEvent(Timestamp receiveTime);

 private:
  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop *loop_;  //事件循环
  const int fd_;     //监听的sockfd
  int revents_;      //返回的事件
  int events_;       // sockfd上监听的事件
  int index_;        // poller的顺序

  std::weak_ptr<void>
      tie_;  //绑定此类对象的弱引用，可用于判断对象是否存活和得到shared指针
  bool tied_;

  //用于处理revents的四种回调函数
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};
}  // namespace muduo