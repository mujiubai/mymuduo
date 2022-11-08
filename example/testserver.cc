#include <mymuduo/Logger.h>
#include <mymuduo/TcpServer.h>

#include <functional>
#include <string>

using namespace muduo;

class EchoServer {
 public:
  EchoServer(EventLoop* loop, const InetAddress& addr, const std::string& name)
      : server_(loop, addr, name), loop_(loop) {
    server_.setConnectionCallback(
        std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&EchoServer::onMessage, this, std::placeholders::_1,
                  std::placeholders::_2, std::placeholders::_3));
    server_.setThreadNum(3);
  }

  void start() { server_.start(); }

 private:
  //连接建立和断开的回调
  void onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
      LOG_INFO("connection UP : [%s] \n",
               conn->peerAddress().toIpPort().c_str());
    } else {
      LOG_INFO("connection DOWN : [%s] \n",
               conn->peerAddress().toIpPort().c_str());
    }
  }
  //可读写事件回调
  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
    std::string msg = buf->retrieveAllAsString();
    conn->send(msg);
    conn->shutdown();  //关闭写端 EPOLLUP => closeCallback_
  }

  EventLoop* loop_;
  TcpServer server_;
};

int main() {
  EventLoop loop;
  InetAddress addr(8000);
  EchoServer server(&loop, addr, "echo");
  server.start();
  loop.loop();
  return 0;
}