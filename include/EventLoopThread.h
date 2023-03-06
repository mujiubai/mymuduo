#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>

#include "Thread.h"
#include "noncopyable.h"

namespace muduo {

class EventLoop;

class EventLoopThread : noncopyable {
 public:
  using ThreadInitCallback = std::function<void(EventLoop *)>;
  EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                  const std::string &name = std::string());
  ~EventLoopThread();

  //创建线程，启动loop
  EventLoop *startLoop();

 private:
  //线程创建时传入的回调函数，里面进行创建loop等操作
  void threadFunc();

  EventLoop *loop_;
  bool exiting_;                  //线程是否正在退出
  Thread thread_;                 //线程
  std::mutex mutex_;              //互斥访问loop_
  std::condition_variable cond_;  //条件变量，用于是否已经成功创建通信
  ThreadInitCallback callback_;  //上层设置的回调函数
};

}  // namespace muduo