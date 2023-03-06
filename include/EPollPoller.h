#pragma once

#include <sys/epoll.h>

#include <vector>

#include "Poller.h"
#include "Timestamp.h"

namespace muduo {

class Channel;

class EPollPoller : public Poller {
 public:
  EPollPoller(EventLoop *loop);
  ~EPollPoller() override;
  
  //重写Poller的抽象方法
  Timestamp poll(int timeoutMs, ChannelList *activeChannels) override;
  void updateChannel(Channel *channel) override;
  void removeChannel(Channel *channel) override;

 private:
  static const int kInitEventListSize = 16;  //给events的初始长度

  //将poll返回的发生事件的channel写入到activeChannels中
  void fillActiveChannels(int numEvents, ChannelList *activeChannels) const;
  //更新channel通道设置的事件
  void update(int operation, Channel *channel);

  using EventList = std::vector<epoll_event>;
  int epollfd_;//epoll的fd
  EventList events_;//记录epoll返回的发生事件
};
}  // namespace muduo