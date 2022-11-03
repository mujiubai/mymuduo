#include "EventLoopThreadPool.h"

#include <memory>

#include "EventLoopThread.h"

using namespace muduo;

EventLoopThreadPool::EventLoopThreadPool(EventLoop *baseLoop,
                                         const std::string &nameArg)
    : baseLoop_(baseLoop),
      name_(nameArg),
      started_(false),
      numThreads_(0),
      next_(0) {}
EventLoopThreadPool::~EventLoopThreadPool() {
  //线程池中创建的eventloop是栈对象，无需手动delete
}

void EventLoopThreadPool::start(const ThreadInitCallback &cb) {
  started_ = true;
  for (int i = 0; i < numThreads_; ++i) {
    char buf[name_.size() + 32];
    snprintf(buf, sizeof buf, "%s%d", name_.c_str(), i);
    EventLoopThread *t = new EventLoopThread(cb, buf);
    threads_.push_back(std::unique_ptr<EventLoopThread>(t));
    //底层创建线程，绑定一个新的Eventloop，并但会该loop地址
    loops_.push_back(t->startLoop());
  }

  //没有创建其他线程，只有一个mainLoop
  if (numThreads_ == 0 && cb) {
    cb(baseLoop_);  //如果用户传了cb则需执行下回调******************
  }
}

//如果工作在多线程中，baseLoop以轮询方式分配channel给subloop
EventLoop *EventLoopThreadPool::getNextLoop() {
  /***********************************************************/
  //如果未创建线程，则返回baseLoop，此时所有工作都在一个线程中执行
  EventLoop *loop = baseLoop_;

  //轮询
  if (!loops_.empty()) {
    loop = loops_[next_];
    ++next_;
    if (next_ >= loops_.size()) {
      next_ = 0;
    }
  }

  return loop;
}

std::vector<EventLoop *> EventLoopThreadPool::getAllLoops() {
  if (loops_.empty()) {
    return std::vector<EventLoop *>(1, baseLoop_);
  } else {
    loops_;
  }
}
