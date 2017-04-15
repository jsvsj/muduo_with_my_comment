#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <set>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;


//多线程服务端的优化
class ChatServer : boost::noncopyable
{
 public:
  ChatServer(EventLoop* loop,
             const InetAddress& listenAddr)
  : loop_(loop),
    server_(loop, listenAddr, "ChatServer"),
    codec_(boost::bind(&ChatServer::onStringMessage, this, _1, _2, _3)),
	//此时智能指针是引用计数为1	
    connections_(new ConnectionList)
  {
    server_.setConnectionCallback(
        boost::bind(&ChatServer::onConnection, this, _1));
	
    server_.setMessageCallback(
        boost::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
  }

  void setThreadNum(int numThreads)
  {
    server_.setThreadNum(numThreads);
  }

  void start()
  {
    server_.start();
  }

 private:
 	//连接状态的回调函数，即在客户端连接与断开都会被调用
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");

    MutexLockGuard lock(mutex_);

	//如果引用计数>1
    if (!connections_.unique())
    {
    //拷贝的一份ConnectionList
    //执行下面一行代码后，原来的列表对象的引用计数-1,new出的新的列表对象的引用计数为1
      connections_.reset(new ConnectionList(*connections_));
    }
    assert(connections_.unique());

//在副本上修改，不会影响原来列表，所以读者在读的时候不需要mutex
//原来的列表对象在读操作结束之后，引用计数会变为0，自动销毁，副本会替代原来的列表对象
//如果原来的引用计数就是一,即没有线程进行读操作，则不会执行上面if语句中的内容，不会产生副本，则下面就会在原来上面修改，
	if (conn->connected())
    {
      connections_->insert(conn);
    }
    else
    {
      connections_->erase(conn);
    }
  }

  typedef std::set<TcpConnectionPtr> ConnectionList;
  typedef boost::shared_ptr<ConnectionList> ConnectionListPtr;

//消息到来的回调函数
//在读端读时，先将引用计数+1，读完后，再-1
  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
  //引用计数+1,mutex保护地范围减小了
    ConnectionListPtr connections = getConnectionList();
  //不受到mutex保护，客户端列表被写着改变了怎么办
  //实际上，写着是在另一个副本上修改，所以无需担心
    for (ConnectionList::iterator it = connections->begin();
        it != connections->end();
        ++it)
    {
    //转发消息
    
    //消息到达第一个客户端与最后一个客户端之间的延迟依旧比较大
      codec_.send(get_pointer(*it), message);
    }

	//栈上的connections销毁，引用计数-1
  }

//获取客户端列表指针
  ConnectionListPtr getConnectionList()
  {
  //加锁
    MutexLockGuard lock(mutex_);
    return connections_;
  }

  EventLoop* loop_;
  TcpServer server_;
  LengthHeaderCodec codec_;
  MutexLock mutex_;
  
  //std::set<TcpConnectionPtr> ConnectionList的shared_ptr智能指针
  ConnectionListPtr connections_;
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

