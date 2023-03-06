#include "Poller.h"
#include "Channel.h"

using namespace muduo;

Poller::Poller(EventLoop *loop) : ownerLoop_(loop) {}

bool Poller::hasChannel(Channel *channel) const {
    auto it=channels_.find(channel->fd());
    //it->second==channel应该是为了避免sockfd被复用的情况
    return it!=channels_.end()&&it->second==channel;
}