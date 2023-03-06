#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "noncopyable.h"

namespace muduo {

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : noncopyable {
 public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;

  EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
  ~EventLoopThreadPool();

  void setThreadNum(int numThreads) { numThreads_ = numThreads; }
  void start(const ThreadInitCallback &cb = ThreadInitCallback());

  //如果工作在多线程中，baseLoop以轮询方式分配channel给subloop
  EventLoop *getNextLoop();

  std::vector<EventLoop *> getAllLoops();

  bool started() const { return started_; }
  const std::string name() const { return name_; }

 private:
  // mainReactor，设置这个成员是考虑轮询算法寻找subLoop时，当不存在subLoop时能将mainLoop作为作为返回结果
  EventLoop *baseLoop_;
  std::string name_;
  bool started_;
  int numThreads_;
  //轮询时下一个位置指向
  int next_;
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  std::vector<EventLoop *> loops_;
};
}  // namespace muduo