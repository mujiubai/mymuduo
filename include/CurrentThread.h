#pragma once

#include <sys/syscall.h>
#include <unistd.h>

namespace muduo {
namespace CurrentThread {

//__thread: thread_local
extern __thread int t_cachedTid;  //每个线程的全局变量

void cacheTid();

inline int tid() {
  //为避免重复换到内核态，每次调用前判断是否已经读过tid值
  if (__builtin_expect(t_cachedTid == 0, 0)) {
    cacheTid();
  }
  return t_cachedTid;
}
}  // namespace CurrentThread
}  // namespace muduo