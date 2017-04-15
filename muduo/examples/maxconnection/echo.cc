#include "echo.h"

#include <muduo/base/Logging.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

EchoServer::EchoServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       int maxConnections)
  : loop_(loop),
    server_(loop, listenAddr, "EchoServer"),
    numConnected_(0),
    kMaxConnections_(maxConnections)
{
  server_.setConnectionCallback(
      boost::bind(&EchoServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&EchoServer::onMessage, this, _1, _2, _3));
}

void EchoServer::start()
{
  server_.start();
}

//连接状态的回调函数
void EchoServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
  //如果是建立连接，将当前连接数+1
    ++numConnected_;

//如果连接数超过限制，则断开连接
    if (numConnected_ > kMaxConnections_)
    {
      conn->shutdown();
    }
  }
  else
  {
    --numConnected_;
  }
  LOG_INFO << "numConnected = " << numConnected_;
}

//消息到来的回调函数
void EchoServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp time)
{
  string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " echo " << msg.size() << " bytes at " << time.toString();
  conn->send(msg);
}

