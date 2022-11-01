#include "Channel.h"

#include <sys/epoll.h>

#include "Logger.h"

using namespace muduo;

//定义事件标识
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false) {}

Channel::~Channel() {}

void Channel::tie(const std::shared_ptr<void> &obj) {
  tie_ = obj;
  tied_ = true;
}

//当改变channel的events后，update负责在poller里面改变fd相应的事件
void Channel::update() {
  //通过channel所属的event loop，调用相应的注册事件方法
  // add code...
  // loop->updateChannel(this);
}

//在所属的eventloop移除此channel
void Channel::remove() {
  // add code.
  // loop_->removeChannel(this);
}

void Channel::HandleEvent(Timestamp receiveTime) {
  if (tied_) {
    std::shared_ptr<void> guard = tie_.lock();
    if (guard) {
    }
  } else {
    handleEventWithGuard(receiveTime);
  }
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
  // LOG_INFO("channel handleEvent revents:%d \n",revents_); 一直警告%d类型不匹配
  LOG_INFO("channel handleEvent revents:\n");
  if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
    if (closeCallback_) {
      closeCallback_();
    }
  }
  if (revents_ & EPOLLERR) {
    if (errorCallback_) {
      errorCallback_();
    }
  }
  if (revents_ & (EPOLLIN | EPOLLPRI)) {
    if (readCallback_) {
      readCallback_(receiveTime);
    }
  }
  if (revents_ & EPOLLOUT) {
    if (writeCallback_) {
      writeCallback_();
    }
  }
}