#include "CurrentThread.h"


namespace muduo {
namespace CurrentThread {

__thread int t_cachedTid(0);

void cacheTid() {
  if (t_cachedTid == 0) {
    //通过linux系统调用获取但钱线程tid值
    t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
  }
}

}  // namespace CurrentThread
}  // namespace muduo