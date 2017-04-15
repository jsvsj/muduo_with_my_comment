#include "echo.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

#include <assert.h>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;


EchoServer::EchoServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       int idleSeconds)
  : loop_(loop),
    server_(loop, listenAddr, "EchoServer"),
    connectionBuckets_(idleSeconds)
{
  server_.setConnectionCallback(
      boost::bind(&EchoServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&EchoServer::onMessage, this, _1, _2, _3));

  //设置定时器，每过一秒钟,回调onTimer函数
  loop->runEvery(1.0, boost::bind(&EchoServer::onTimer, this));

  connectionBuckets_.resize(idleSeconds);

  dumpConnectionBuckets();
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
  //连接到来,创建一个entry
    EntryPtr entry(new Entry(conn));//此时entry引用计数为一
    
    connectionBuckets_.back().insert(entry);//插入到队尾,entry引用计数为2

//打印引用计数
	dumpConnectionBuckets();

//弱引用
	WeakEntryPtr weakEntry(entry);

//将若引用和conn相关联
	conn->setContext(weakEntry);

  //entry在栈上，销毁，entry引用计数变为一

  }
  else
  {
  //连接断开
    assert(!conn->getContext().empty());

  //获得关联的弱引用
    WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));

//打印引用计数
	LOG_DEBUG << "Entry use_count = " << weakEntry.use_count();
  }
}

//消息到来
void EchoServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp time)
{
  string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " echo " << msg.size()
           << " bytes at " << time.toString();
  //发送消息
  conn->send(msg);

  assert(!conn->getContext().empty());

  //获得关联的弱引用
  WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));

//提升弱引用为share_ptr,此时entry引用计数变为2
  EntryPtr entry(weakEntry.lock());

  if (entry)
  {
  //插入到尾部
    connectionBuckets_.back().insert(entry);//引用计数变为3

//打印引用计数
	dumpConnectionBuckets();
  }
  //引用计数又变为2
}

//每过一秒钟执行的回调函数
void EchoServer::onTimer()
{
//相当于将tail位置原有的bucket删除了，再增加了一个空的bucket

//其中存放的entry的引用计数全部-1
  connectionBuckets_.push_back(Bucket());
//当引用计数减为0是entry析构，析构函数中调用shutdown()函数断开连接
	
//打印引用计数
  dumpConnectionBuckets();
}

//将环形缓冲区中的连接的引用计数打印
void EchoServer::dumpConnectionBuckets() const
{
  LOG_INFO << "size = " << connectionBuckets_.size();
  int idx = 0;
  //遍历环形缓冲区
  for (WeakConnectionList::const_iterator bucketI = connectionBuckets_.begin();
      bucketI != connectionBuckets_.end();
      ++bucketI, ++idx)
  {
  //得到格子中的set
    const Bucket& bucket = *bucketI;
    printf("[%d] len = %zd : ", idx, bucket.size());

//遍历set
	for (Bucket::const_iterator it = bucket.begin();
        it != bucket.end();
        ++it)
    {
      bool connectionDead = (*it)->weakConn_.expired();
      printf("%p(%ld)%s, ", get_pointer(*it), it->use_count(),
          connectionDead ? " DEAD" : "");
    }
    puts("");
  }
}

