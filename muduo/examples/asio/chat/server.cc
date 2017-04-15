#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <set>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

class ChatServer : boost::noncopyable
{
 public:
  ChatServer(EventLoop* loop,
             const InetAddress& listenAddr)
  : loop_(loop),
    server_(loop, listenAddr, "ChatServer"),
    codec_(boost::bind(&ChatServer::onStringMessage, this, _1, _2, _3))
  {
  //设置连接状态的回调函数
    server_.setConnectionCallback(
        boost::bind(&ChatServer::onConnection, this, _1));

  //设置消息到来的回调函数
	server_.setMessageCallback(
	//注意回调的是LengthHeaderCodec::onMessage
        boost::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
  }

  void start()
  {
    server_.start();
  }

 private:
 	//连接状态的回调函数
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

//如果是建立连接
//由于只有一个IO线程，故不需要Mutex保护
    if (conn->connected())
    {
    //将连接插入到连接列表中
      connections_.insert(conn);
    }
	//如果是断开连接
    else
    {
    //移除连接
      connections_.erase(conn);
    }
  }

//消息到来的回调函数,被LengthHeaderCodec::onMessage函数回调
  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    for (ConnectionList::iterator it = connections_.begin();
        it != connections_.end();
        ++it)
    {
    //先编码，即加上包头，表示消息长度，再发送
      codec_.send(get_pointer(*it), message);
    }
  }

  typedef std::set<TcpConnectionPtr> ConnectionList;
  
  EventLoop* loop_;
  TcpServer server_;

  //消息编解码
  LengthHeaderCodec codec_;

  //连接列表
  ConnectionList connections_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    EventLoop loop;

	//参数指定了端口号
	uint16_t port = static_cast<uint16_t>(atoi(argv[1]));

	InetAddress serverAddr(port);
    ChatServer server(&loop, serverAddr);
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s port\n", argv[0]);
  }
}

