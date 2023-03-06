#pragma once

#include <memory>

namespace muduo {

class Buffer;
class TcpConnection;
class Timestamp;

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
// HighWaterMarkCallback:警戒线回调。比如当发送数据过快，接收方来不及接受时，此时发送方需要进行处理比如需要停止发送，
//调用此回调就会调用用户定义的处理方法
using HighWaterMarkCallback = std::function <
                              void(const TcpConnectionPtr&, size_t)>;
using MessageCallback =
    std::function<void(const TcpConnectionPtr&, Buffer*, Timestamp)>;
}  // namespace muduo