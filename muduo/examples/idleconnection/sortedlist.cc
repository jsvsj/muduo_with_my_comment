#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>
#include <boost/bind.hpp>
#include <list>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// RFC 862
class EchoServer
{
 public:
  EchoServer(EventLoop* loop,
             const InetAddress& listenAddr,
             int idleSeconds);

  void start()
  {
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn);

  void onMessage(const TcpConnectionPtr& conn,
                 Buffer* buf,
                 Timestamp time);

  void onTimer();

  void dumpConnectionList() const;

  typedef boost::weak_ptr<TcpConnection> WeakTcpConnectionPtr;
  typedef std::list<WeakTcpConnectionPtr> WeakConnectionList;


  struct Node : public muduo::copyable
  {
  //该连接最后一次活跃的时间
    Timestamp lastReceiveTime;
  //该连接在列表中的位置
    WeakConnectionList::iterator position;
  };


  EventLoop* loop_;
  TcpServer server_;

  //超时时间
  int idleSeconds_;

  /连接列表,有序排列
  WeakConnectionList connectionList_;
};

EchoServer::EchoServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       int idleSeconds)
  : loop_(loop),
    server_(loop, listenAddr, "EchoServer"),
    idleSeconds_(idleSeconds)
{
  server_.setConnectionCallback(
      boost::bind(&EchoServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&EchoServer::onMessage, this, _1, _2, _3));

  //设置定时器
  loop->runEvery(1.0, boost::bind(&EchoServer::onTimer, this));
  dumpConnectionList();
}

//连接状态的回调函数
void EchoServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
  //如果是建立连接
    Node node;
  //连接最后一次活跃的时间
    node.lastReceiveTime = Timestamp::now();

//将连接插入到列表的尾部
	connectionList_.push_back(conn);

//记录连接在列表中的位置
	node.position = --connectionList_.end();

//将node与conn关联 
	conn->setContext(node);
  }
  else
  {
  //连接断开
    assert(!conn->getContext().empty());

	const Node& node = boost::any_cast<const Node&>(conn->getContext());

//从列表中移除连接 
	connectionList_.erase(node.position);
  }
  
  dumpConnectionList();
}

//消息到来的回调函数
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

//获得连接中关联的node
  Node* node = boost::any_cast<Node>(conn->getMutableContext());

//改变最后一次活跃的时间
  node->lastReceiveTime = time;
  // move node inside list with list::splice()

//移除原来的
  connectionList_.erase(node->position);

//添加到末尾
  connectionList_.push_back(conn);

//改变位置
  node->position = --connectionList_.end();
//打印连接
  dumpConnectionList();
}

//每过一秒的回调函数
void EchoServer::onTimer()
{
  dumpConnectionList();
  
  Timestamp now = Timestamp::now();

//遍历连接列表,该列表是，最后的一次的活跃时间越晚排在越后
  for (WeakConnectionList::iterator it = connectionList_.begin();
      it != connectionList_.end();)
  {
  //提升
    TcpConnectionPtr conn = it->lock();
    if (conn)
    {
      Node* n = boost::any_cast<Node>(conn->getMutableContext());
	  //获得时间差
      double age = timeDifference(now, n->lastReceiveTime);
//如果超时了，断开连接
	  if (age > idleSeconds_)
      {
        conn->shutdown();
      }
	  //这种情况一般不会发生
      else if (age < 0)
      {
        LOG_WARN << "Time jump";
        n->lastReceiveTime = now;
      }
      else//未超时
      {
        break;
      }
      ++it;
    }
	//说明连接关闭
    else
    {
      LOG_WARN << "Expired";
	  //移除
      it = connectionList_.erase(it);
    }
  }
}

//打印连接列表
void EchoServer::dumpConnectionList() const
{
  LOG_INFO << "size = " << connectionList_.size();

  for (WeakConnectionList::const_iterator it = connectionList_.begin();
      it != connectionList_.end(); ++it)
  {
    TcpConnectionPtr conn = it->lock();
    if (conn)
    {
      printf("conn %p\n", get_pointer(conn));
      const Node& n = boost::any_cast<const Node&>(conn->getContext());
      printf("    time %s\n", n.lastReceiveTime.toString().c_str());
    }
    else
    {
      printf("expired\n");
    }
  }
}

int main(int argc, char* argv[])
{
  EventLoop loop;
  InetAddress listenAddr(2007);
  int idleSeconds = 10;
  if (argc > 1)
  {
    idleSeconds = atoi(argv[1]);
  }
  LOG_INFO << "pid = " << getpid() << ", idle seconds = " << idleSeconds;
  EchoServer server(&loop, listenAddr, idleSeconds);
  server.start();
  loop.loop();
}

