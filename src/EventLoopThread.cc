#include "EventLoopThread.h"

#include "EventLoop.h"

using namespace muduo;

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const std::string &name)
    : loop_(nullptr),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),
      mutex_(),
      cond_(),
      callback_(cb) {}

EventLoopThread::~EventLoopThread() {
  exiting_ = true;
  if (loop_ != nullptr) {
    loop_->quit();
    thread_.join();
  }
}

EventLoop *EventLoopThread::startLoop() {
  thread_.start();  //启动低层线程

  EventLoop *loop = nullptr;

  {
    std::unique_lock<std::mutex> lock(mutex_);
    while (loop_ == nullptr) {
      cond_.wait(lock);
    }
    loop = loop_;
  }
  return loop;
}

//此函数在单独新线程里面运行
void EventLoopThread::threadFunc() {
  //*******//
  //创建一个独立的Eventloop，和线程是一一对应 one loop per thread
  EventLoop loop;

  if (callback_) {
    callback_(&loop);
  }

  {
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = &loop;
    cond_.notify_one();  //通知已经startLoop函数成功启动进程
  }

  loop.loop();  //开启低层poller的poll函数

  std::unique_lock<std::mutex> lock(mutex_);
  loop_ = nullptr;
}
