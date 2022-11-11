#pragma once

#include <unistd.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "noncopyable.h"

namespace muduo {
class Thread : noncopyable {
 public:
  using ThreadFunc = std::function<void()>;

  explicit Thread(ThreadFunc, const std::string& name = std::string());
  ~Thread();

  //启动线程
  void start();
  void join();

  //获取线程是否启动
  bool started() const { return started_; }
  //获取线程id
  pid_t tid() const { return tid_; }
  //获取设置的线程名
  const std::string& name() const { return name_; }

  //获取当前通过Thread创建的线程数量
  static int numCreated() { return numCreated_; }

 private:
  void setDefaultName();

  bool started_;  //是否已经启动
  bool joined_;   //是否join
  //使用智能指针，避免使用thread初始化是就开始启动线程
  std::shared_ptr<std::thread> thread_;
  pid_t tid_;         //记录线程id
  ThreadFunc func_;   //线程执行的回调函数
  std::string name_;  //线程名

  static std::atomic_int numCreated_;  //现有线程创建个数
};
}  // namespace muduo