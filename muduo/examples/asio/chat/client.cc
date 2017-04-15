#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/TcpClient.h>

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>

#include <iostream>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

//客户端类
class ChatClient : boost::noncopyable
{
 public:
  ChatClient(EventLoop* loop, const InetAddress& serverAddr)
    : loop_(loop),
      client_(loop, serverAddr, "ChatClient"),
      codec_(boost::bind(&ChatClient::onStringMessage, this, _1, _2, _3))
  {
  //设置连接状态的回调函数
    client_.setConnectionCallback(
        boost::bind(&ChatClient::onConnection, this, _1));
//设置消息到来的回调函数
//注意回调的是LengthHeaderCodec::onMessage,此函数解包后又回调ChatClient::onStringMessage
	client_.setMessageCallback(
        boost::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
//
	client_.enableRetry();
  }

  void connect()
  {
    client_.connect();
  }

  void disconnect()
  {
    // client_.disconnect();
  }

//发送消息
  void write(const StringPiece& message)
  {
    MutexLockGuard lock(mutex_);
    if (connection_)
    {
      codec_.send(get_pointer(connection_), message);
    }
  }

 private:
 	//连接状态的回调函数
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    MutexLockGuard lock(mutex_);
    if (conn->connected())
    {
      connection_ = conn;
    }
    else
    {
      connection_.reset();
    }
  }

//消息到来的回调函数
  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    printf("<<< %s\n", message.c_str());
  }

  EventLoop* loop_;

  TcpClient client_;

  LengthHeaderCodec codec_;

  MutexLock mutex_;
  
  TcpConnectionPtr connection_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 2)
  {
    EventLoopThread loopThread;

	//参数指定端口
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));

	//参数指定IP地址
	InetAddress serverAddr(argv[1], port);

    ChatClient client(loopThread.startLoop(), serverAddr);

	client.connect();
    std::string line;

// 主线程接收从键盘输入的消息，IO线程接收服务端发送过来的消息
	while (std::getline(std::cin, line))
    {
      client.write(line);
    }

	//断开连接
    client.disconnect();
  }
  else
  {
    printf("Usage: %s host_ip port\n", argv[0]);
  }
}

