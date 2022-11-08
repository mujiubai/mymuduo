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
  // fd得到poller通知以后，处理事件的函数
  void HandleEvent(Timestamp receiveTime);

  //设置回调函数
  void setReadCallback(ReadEventCallback cb) {
    readCallback_ = std::move(cb);
  }  //这里使用move感觉没啥用，除非function中好有申请的资源
  void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

  //防止当channel被手动remove后，channel还在执行回调操作,暂不知道在哪调用
  void tie(const std::shared_ptr<void> &);

  int fd() const { return fd_; };
  int events() const { return events_; }
  int setRevents(int revt) { revents_ = revt; }  //供poller设置发生事件

  //设置或取消读 写事件:
  void enableReading() {
    events_ |= kReadEvent;
    update();
  }
  void disableReading() {
    events_ &= ~kReadEvent;
    update();
  }
  void enableWriting() {
    events_ |= kWriteEvent;
    update();
  }
  void disableWriting() {
    events_ &= ~kWriteEvent;
    update();
  }
  void disableAll() {
    events_ = kNoneEvent;
    update();
  }

  //判断当前事件状态，是否设置读或写事件
  bool isWriting() const { return events_ & kWriteEvent; }
  bool isReading() const { return events_ & kReadEvent; }
  bool isNoneEvent() const { return events_ == kNoneEvent; }

  int index() const { return index_; }
  void setIndex(int index) { index_ = index; }
   
   //返回此channel所属的eventloop
   EventLoop* ownerLoop(){return loop_;}
   //在所属的eventloop移除此channel
   void remove();

 private:
    void update();//更新poller中对应的channel的事件
    //安全的处理事件，作为handleEvent函数的底层函数
    void handleEventWithGuard(Timestamp receiveTime);

  static const int kNoneEvent;  //这三个是作为事件标识
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop *loop_;  //事件循环
  const int fd_;     //监听的sockfd
  int revents_;      //返回的事件
  int events_;       // sockfd上监听的事件
  int index_;        // poller的顺序

  //绑定TcpConnection对象的弱引用，可用于判断对象是否存活和得到shared指针
  //用于观察注册回调的TcpConnection对象是否存活，避免调用回调函数时出错
  std::weak_ptr<void> tie_;  
  bool tied_;

  //用于处理revents的四种回调函数
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};
}  // namespace muduo