#include "Thread.h"

#include <semaphore.h>

#include "CurrentThread.h"

using namespace muduo;

std::atomic_int Thread::numCreated_(0);  //不能使用等于0来赋值

Thread::Thread(ThreadFunc func, const std::string& name)
    : started_(false),
      joined_(false),
      tid_(0),
      func_(std::move(func)),
      name_(name) {
  setDefaultName();
}

Thread::~Thread() {
  if (started_ && !joined_) {
    thread_->detach();  // thread类提供的设置分离线程的方法
  }
}

//一个thread对象记录的就是一个新线程的详细信息
void Thread::start() {
  started_ = true;
  sem_t sem;  //信号量 muduo书中说多线程别用信号量？？
  sem_init(&sem, false, 0);
  //开启线程，
  thread_ = std::shared_ptr<std::thread>(new std::thread([&]() {
    //获取线程的tid值
    tid_ = CurrentThread::tid();
    sem_post(&sem);
    //开启一个新线程，专门执行该线程函数
    func_();
  }));

  //必须等待上面新创建的线程的tid值
  sem_wait(&sem);
}

void Thread::join() {
  joined_ = true;
  thread_->join();
}

void Thread::setDefaultName() {
  int num = ++numCreated_;
  if (name_.empty()) {
    char buf[32] = {0};
    snprintf(buf, sizeof buf, "Thread%d", num);
    name_ = buf;
  }
}