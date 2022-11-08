#pragma once

#include <unordered_map>
#include <vector>

#include "Timestamp.h"
#include "noncopyable.h"

namespace muduo {
class Channel;
class EventLoop;
// muduo中多路事件分发器核心 IO复用模块
class Poller : noncopyable {
 public:
  using ChannelList = std::vector<Channel *>;
  Poller(EventLoop *loop);
  virtual ~Poller()=default;

  //统一IO复用接口
  virtual Timestamp poll(int timeousMs, ChannelList *activeChannels) = 0;
  virtual void updateChannel(Channel *channel) = 0;
  virtual void removeChannel(Channel *channel) = 0;

  //判断channel是否存在此poller中
  bool hasChannel(Channel *channel) const;

  // eventloop通过该接口获取默认的IO复用的具体实现
  //为避免基类引用派生类头文件，此函数是在一个单独的cc文件中进行实现
  static Poller *newDefaultPoller(EventLoop *loop);

 protected:
  // key:sockfd value:sockfd所属的channel通道类型
  using ChannelMap = std::unordered_map<int, Channel *>;
  ChannelMap channels_;

 private:
  EventLoop *ownerLoop_;
};
}  // namespace muduo