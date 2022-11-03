#include "EventLoop.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>

#include "Channel.h"
#include "Logger.h"
#include "Poller.h"

using namespace muduo;

//防止一个线程创建多个eventloop __thread:gcc内置类似thread__local机制
__thread EventLoop *t_loopInThisThread = nullptr;

//默认poller io复用超时事件
const int kPollTimeMs = 10000;

//创建wakeupFd，用来唤醒subReactor处理新来的Channel
int createEventFd() {
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0) {
    LOG_FATAL("Failed in eventfd, eventfd error:%d \n", errno);
  }
  return evtfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      callingPendingFuctors_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      wakeupFd_(createEventFd()),
      wakeupChanel_(new Channel(this, wakeupFd_)),
      currentActiveChannel_(nullptr) {
  LOG_DEBUG("EventLoop created %p in thread %d \n", this, threadId_);
  if (t_loopInThisThread) {
    LOG_FATAL("Another eventloop %p exists in this thread %d \n",
              t_loopInThisThread, threadId_);
  } else {
    t_loopInThisThread = this;
  }

  //设置wakeupFd事件类型和发生事件后的回调
  wakeupChanel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
  //每一个eventloop都监听wakeupchannel的EPOLLIN事件
  wakeupChanel_->enableReading();
}

EventLoop::~EventLoop() {
  wakeupChanel_->disableAll();
  wakeupChanel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = nullptr;
}

//开启事件循环
void EventLoop::loop() {
  looping_ = true;
  quit_ = false;
  LOG_INFO("EventLoop %p start looping \n", this);
  while (!quit_) {
    activeChannels_.clear();
    //监听两种fd：clientfd和wakeupfd
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
    for (Channel *channel : activeChannels_) {
      // Poller监听哪些channel发生事件，上报给eventloop后通知channel处理事件
      channel->HandleEvent(pollReturnTime_);
    }

    /**
     * mainLoop在唤醒subReactor后，需要subReactor做事，比如执行新用户交互等，而mainLoop需要subReactor执行的操作会写在一个函数中，注册为subReactor的回调
     */
    //执行当前eventloop事件循环需要处理的回调操作
    doPendingFunctors();
  }

  LOG_INFO("EventLoop %p stop looping \n", this);
}

//退出事件循环 1.loop在自己线程中调用quit 2.在非当前loop线程中调用loop的quit
void EventLoop::quit() {
  quit_ = true;

  //如果是在其他线程如subloop中调用mainLoop的quit，需要将mainloop唤醒
  if (!isInLoopThread()) {
    wakeup();
  }
}

void EventLoop::handleRead() {
  uint64_t one = 1;
  ssize_t n = read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOG_ERROR("EventLoop::handleRead() reads %d bytes instead of 8", static_cast<int>(n));
  }
}

//在当前loop中执行
void EventLoop::runInLoop(Functor cb) {
  if (isInLoopThread()) {  //在当前loop线程中执行cb
    cb();
  } else {  //在非当前loop线程中执行cb，需要唤醒loop线程，执行cb
    queueInLoop(cb);
  }
}

//把cb放入队列中，唤醒loop所在的线程，再执行cb
void EventLoop::queueInLoop(Functor cb) {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    pendingFuctors_.emplace_back(cb);
  }

  //唤醒loop线程，
  //*****************//
  // ||callingPendingFuctors_的意思是：当前loop正在执行回调，但loop又有了新的回调，那么loop执行完回调后又被阻塞在poll上，需要添加个wakeup将其唤醒
  if (!isInLoopThread() ||
      callingPendingFuctors_) {  // callingPendingFuctors_逻辑待解释
    wakeup();                    //唤醒loop所在线程
  }
}

//唤醒loop线程，想wakeupfd_写入一个数据,wakeupchannel发生读事件，当前loop就会被唤醒
void EventLoop::wakeup() {
  uint64_t one = 1;
  ssize_t n = write(wakeupFd_, &one, sizeof one);
  if (n != one) {
    LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8  \n", n);
  }
}

// eventloop使用poller对channel操作的方法
void EventLoop::updateChannel(Channel *channel) {
  poller_->updateChannel(channel);
}
void EventLoop::removeChannel(Channel *channel) {
  poller_->removeChannel(channel);
}
bool EventLoop::hasChannel(Channel *channel) { poller_->hasChannel(channel); }

//执行回调
void EventLoop::doPendingFunctors() {
  std::vector<Functor> functors;  //避免执行回调时插入functor导致等待
  callingPendingFuctors_ = true;
  {
    std::unique_lock<std::mutex> lock(mutex_);
    functors.swap(pendingFuctors_);
  }
  for (const Functor &functor : functors) {
    functor();  //执行当前loop需要执行的回调
  }
  callingPendingFuctors_ = false;
}