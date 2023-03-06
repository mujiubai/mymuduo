#include "EPollPoller.h"

#include <errno.h>
#include <memory.h>
#include <unistd.h>

#include "Channel.h"
#include "Logger.h"

using namespace muduo;

// channel未添加到poller中
const int kNew = -1;  // channel 成员初始时index_=-1,
// channel已添加到poller中
const int kAdded = 1;
// channel已从poller中删除
const int kDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize)  // vector<epoll_event> 创建
{
  if (epollfd_ < 0) {
    LOG_FATAL("epoll_create error:%d \n", errno);
  }
}

EPollPoller::~EPollPoller() { ::close(epollfd_); }

//对应epoll_wait操作
Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels) {
  //使用LOG_DEBUG更合理
  LOG_INFO("func=%s => fd total count:%d \n", __FUNCTION__, static_cast<int>(channels_.size()));
  int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                               static_cast<int>(events_.size()), timeoutMs);
  int saveError = errno;  //防止多线程时覆盖errno
  Timestamp now(Timestamp::now());
  if (numEvents > 0) {
    LOG_INFO("%d events happended \n", numEvents);
    fillActiveChannels(numEvents, activeChannels);
    if (numEvents == events_.size()) {  // events 扩容
      events_.resize(events_.size() * 2);
    }
  } else if (numEvents == 0) {
    LOG_DEBUG("%s timeout \n", __FUNCTION__);
  } else {
    if (saveError == EINTR) {
      errno = saveError;
      LOG_ERROR("EPollPoll::poller() error!\n");
    }
  }
  return now;
}

//被eventloop调用 更新channel
// Eventloop中包含ChannelList和poller
void EPollPoller::updateChannel(Channel *channel) {
  const int index = channel->index();
  LOG_INFO("func=%s => fd=%d events=%d index=%d \n", __FUNCTION__,
           channel->fd(), channel->events(), index);
  if (index == kNew || index == kDeleted) {
    if (index == kNew) {
      channels_[channel->fd()] = channel;
    }
    channel->setIndex(kAdded);
    update(EPOLL_CTL_ADD, channel);
  } else {  // channel已经在poller上注册过
    int fd = channel->fd();
    if (channel->isNoneEvent()) {
      update(EPOLL_CTL_DEL, channel);
      channel->setIndex(kDeleted);
    } else {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

//从epollpoller中删除channel
void EPollPoller::removeChannel(Channel *channel) {
  int fd = channel->fd();
  int index = channel->index();
  size_t n = channels_.erase(fd);
  LOG_INFO("func=%s => fd=%d\n", __FUNCTION__, channel->fd());
  if (index == kAdded) {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->setIndex(kDeleted);
}

//填写活跃的链接
void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList *activeChannels) const {
  for (int i = 0; i < numEvents; ++i) {
    Channel *channel = static_cast<Channel *>(events_[i].data.ptr);
    channel->setRevents(events_[i].events);
    // eventloop拿到poller返回的所有事件的channel列表
    activeChannels->push_back(channel);
  }
}

//更新channel通道 epoll_ctl add/mod/del
void EPollPoller::update(int operation, Channel *channel) {
  epoll_event event;
  bzero(&event, sizeof event);
  int fd = channel->fd();

  event.events = channel->events();
  event.data.fd = fd;
  event.data.ptr = channel;  //以后返回的事件可以通过ptr指针访问对应的fd对象

  if (epoll_ctl(epollfd_, operation, fd, &event) < 0) {
    if (operation == EPOLL_CTL_DEL) {
      LOG_ERROR("epoll_ctl del error:%d \n", errno);
    } else {
      LOG_FATAL("epoll_ctl add/mod error:%d \n", errno);
    }
  }
}
