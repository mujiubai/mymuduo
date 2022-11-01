#include <stdlib.h>

#include "Poller.h"
using namespace muduo;

//为避免基类引用派生类头文件，所以此函数单独一个文件写
Poller *Poller::newDefaultPoller(EventLoop *loop) {
  if (::getenv("MUDUO_USE_POLL")) {
    return nullptr;  //生成poll实例
  } else {
    return nullptr;  //生成epoll实例
  }
}