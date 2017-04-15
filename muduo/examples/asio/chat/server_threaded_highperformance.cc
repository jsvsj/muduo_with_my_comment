#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/ThreadLocalSingleton.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

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
    server_.setConnectionCallback(
        boost::bind(&ChatServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
  }

//设置线程个数
  void setThreadNum(int numThreads)
  {
    server_.setThreadNum(numThreads);
  }

  void start()
  {
    server_.setThreadInitCallback(boost::bind(&ChatServer::threadInit, this, _1));
    server_.start();
  }

 private:
 	//连接状态的回调函数
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

//注意，这里不需要mutex保护，因为每个线程都有各自的connection_对象
    if (conn->connected())
    {
      connections_.instance().insert(conn);
    }
    else
    {
      connections_.instance().erase(conn);
    }
  }

//消息到来的回调函数
  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    EventLoop::Functor f = boost::bind(&ChatServer::distributeMessage, this, message);
    LOG_DEBUG;

    MutexLockGuard lock(mutex_);
	//转发消息给所有用户,多线程转发，高效
	//遍历的是线程的vector
    for (std::set<EventLoop*>::iterator it = loops_.begin();
        it != loops_.end();
        ++it)
    {
    //1,让客户端对应的IO线程来执行distributeMessage
	//2.distributeMessage放到IO线程中执行，因此这里的mutex竞争大大减小，因为这里的执行时间缩短
	//3.distributeMessage不收mutex保护
	 (*it)->queueInLoop(f);
    }
    LOG_DEBUG;
  }

  typedef std::set<TcpConnectionPtr> ConnectionList;

//让IO线程调用此函数转发消息
  void distributeMessage(const string& message)
  {
    LOG_DEBUG << "begin";
    for (ConnectionList::iterator it = connections_.instance().begin();
        it != connections_.instance().end();
        ++it)
    {
      codec_.send(get_pointer(*it), message);
    }
    LOG_DEBUG << "end";
  }

//初始化线程时的回调函数
  void threadInit(EventLoop* loop)
  {
    assert(connections_.pointer() == NULL);
//为每一个创建的线程创建connection_对象
	connections_.instance();
	
    assert(connections_.pointer() != NULL);
    MutexLockGuard lock(mutex_);
//将创建的loop添加到loops_中
	loops_.insert(loop);
  }

  EventLoop* loop_;
  TcpServer server_;
  LengthHeaderCodec codec_;

  //线程局部单例变量
  //connections_，每个线程都有一个connections_的实例
  ThreadLocalSingleton<ConnectionList> connections_;

  MutexLock mutex_;

  //EventLoop列表
  std::set<EventLoop*> loops_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    EventLoop loop;
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    InetAddress serverAddr(port);
    ChatServer server(&loop, serverAddr);
    if (argc > 2)
    {
      server.setThreadNum(atoi(argv[2]));
    }
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s port [thread_num]\n", argv[0]);
  }
}


