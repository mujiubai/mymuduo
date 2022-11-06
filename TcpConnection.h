#pragma once

#include <atomic>
#include <memory>
#include <string>

#include "Buffer.h"
#include "Callbacks.h"
#include "InetAddress.h"
#include "Timestamp.h"
#include "noncopyable.h"

namespace muduo {

class Channel;
class EventLoop;
class Socket;

/**
 * @brief TcpConnection用来打包成功连接的通信链路
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * => TcpConnection 设置回调 => channel =>poller => channel的回调
 */
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
 public:
  TcpConnection(EventLoop *loop, const std::string &name, int sockfd,
                const InetAddress &localAddr, const InetAddress &peerAddr);
  ~TcpConnection();

  EventLoop *getLoop() const { return loop_; }
  const std::string &name() const { return name_; }
  const InetAddress &localAddress() const { return localAddr_; }
  const InetAddress &peerAddress() const { return peerAddr_; }

  bool connected() const { return state_ == kConnected; }

  //发送数据
  void send(const std::string &buf);
  // void send(const void *message, int len);
  //关闭连接
  void shutdown();

  //设置回调函数
  void setConnectionCallback(const ConnectionCallback &cb) {
    connectionCallback_ = cb;
  }
  void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
  void setWriteCompleteCallback(const WriteCompleteCallback &cb) {
    writeCompleteCallback_ = cb;
  }
  void setHighWaterMarkCallback(const HighWaterMarkCallback &cb) {
    highWaterMarkCallback_ = cb;
  }
  void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }

  //连接建立时的回调函数
  void connectEstablished();
  //连接销毁的回调函数
  void connectDestroyed();

 private:
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
  void setState(StateE state) { state_ = state; }

  //相比于用户注册的各种callback，由于加了一个Buffer层，因此需要操作buffer层再调用对应回调
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();

  void sendInLoop(const void *message, size_t len);

  void shutdownInLoop();

  EventLoop *loop_;  // subLoop地址
  const std::string name_;
  std::atomic_int state_;
  bool reading_;  //可读标志

  std::unique_ptr<Socket> socket_;
  std::unique_ptr<Channel> channel_;

  const InetAddress localAddr_;  //本地地址
  const InetAddress peerAddr_;   //对端地址

  ConnectionCallback connectionCallback_;        //有新连接时的回调
  MessageCallback messageCallback_;              //有读写消息的回调
  WriteCompleteCallback writeCompleteCallback_;  //消息发送完成的回调
  // HighWaterMarkCallback:读写数据达到警戒线的回调。比如当发送数据过快，接收方来不及接受时，此时发送方需要进行处理比如需要停止发送，
  HighWaterMarkCallback highWaterMarkCallback_;
  CloseCallback closeCallback_;

  size_t highWaterMark_;  //水位线标志，超过此数则表示达到警戒线

  Buffer inputBuffer_;   //接受数据缓冲
  Buffer outputBuffer_;  //发送数据缓冲
};
}  // namespace muduo