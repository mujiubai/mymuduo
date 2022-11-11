#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "CurrentThread.h"
#include "Timestamp.h"
#include "noncopyable.h"

//事件循环类， 主要包括channel 和poller（epoll的抽象）
namespace muduo {

class Channel;
class Poller;

/*
 * Reactor
 */
class EventLoop {
 public:
  //即使要传参数，也可以通过bind绑定
  using Functor = std::function<void()>;

  EventLoop();
  ~EventLoop();

  //开启事件循环
  void loop();
  //退出事件循环
  void quit();

  Timestamp pollReturnTime() const { return pollReturnTime_; }

  //在当前loop中执行
  void runInLoop(Functor cb);
  //把cb放入队列中，唤醒loop所在的线程，再执行cb
  void queueInLoop(Functor cb);

  //唤醒loop所在线程
  void wakeup();

  // eventloop使用poller对channel操作的方法
  void updateChannel(Channel *channel);
  void removeChannel(Channel *channel);
  bool hasChannel(Channel *channel);

  //判断eventloop对象是否在自己线程中
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }

 private:
  void handleRead();         // wakeupfd_的读回调函数
  void doPendingFunctors();  //执行需要执行的回调操作

  using ChannelList = std::vector<Channel *>;

  std::atomic_bool looping_;  //原子操作，通过CAS实现
  std::atomic_bool quit_;     //标志是否退出loop循环
  const pid_t threadId_;      //当前loop所在线程id
  Timestamp pollReturnTime_;  // poller返回发生事件的时间点
  std::unique_ptr<Poller> poller_;

  //唤醒当前loop所在线程的fd
  int wakeupFd_;  //当mainLoop获取新用户channel时，通过轮询算法选择一个subloop，通过该成员唤醒subLoop处理channel
  std::unique_ptr<Channel> wakeupChanel_;  // wakeupfd_的channel的指针

  ChannelList activeChannels_;  //记录发生事件的所有channel
  Channel *currentActiveChannel_;

  std::atomic_bool callingPendingFuctors_;  //当前loop是否有需要执行的回调操作
  std::vector<Functor> pendingFuctors_;  //存储loop需要执行的所有回调操作
  std::mutex mutex_;  //保护pendingFuctors_的线程安全操作
};
}  // namespace muduo