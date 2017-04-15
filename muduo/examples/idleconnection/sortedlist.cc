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
  //���������һ�λ�Ծ��ʱ��
    Timestamp lastReceiveTime;
  //���������б��е�λ��
    WeakConnectionList::iterator position;
  };


  EventLoop* loop_;
  TcpServer server_;

  //��ʱʱ��
  int idleSeconds_;

  /�����б�,��������
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

  //���ö�ʱ��
  loop->runEvery(1.0, boost::bind(&EchoServer::onTimer, this));
  dumpConnectionList();
}

//����״̬�Ļص�����
void EchoServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
  //����ǽ�������
    Node node;
  //�������һ�λ�Ծ��ʱ��
    node.lastReceiveTime = Timestamp::now();

//�����Ӳ��뵽�б��β��
	connectionList_.push_back(conn);

//��¼�������б��е�λ��
	node.position = --connectionList_.end();

//��node��conn���� 
	conn->setContext(node);
  }
  else
  {
  //���ӶϿ�
    assert(!conn->getContext().empty());

	const Node& node = boost::any_cast<const Node&>(conn->getContext());

//���б����Ƴ����� 
	connectionList_.erase(node.position);
  }
  
  dumpConnectionList();
}

//��Ϣ�����Ļص�����
void EchoServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp time)
{
  string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " echo " << msg.size()
           << " bytes at " << time.toString();
//������Ϣ
  conn->send(msg);

  assert(!conn->getContext().empty());

//��������й�����node
  Node* node = boost::any_cast<Node>(conn->getMutableContext());

//�ı����һ�λ�Ծ��ʱ��
  node->lastReceiveTime = time;
  // move node inside list with list::splice()

//�Ƴ�ԭ����
  connectionList_.erase(node->position);

//��ӵ�ĩβ
  connectionList_.push_back(conn);

//�ı�λ��
  node->position = --connectionList_.end();
//��ӡ����
  dumpConnectionList();
}

//ÿ��һ��Ļص�����
void EchoServer::onTimer()
{
  dumpConnectionList();
  
  Timestamp now = Timestamp::now();

//���������б�,���б��ǣ�����һ�εĻ�Ծʱ��Խ������Խ��
  for (WeakConnectionList::iterator it = connectionList_.begin();
      it != connectionList_.end();)
  {
  //����
    TcpConnectionPtr conn = it->lock();
    if (conn)
    {
      Node* n = boost::any_cast<Node>(conn->getMutableContext());
	  //���ʱ���
      double age = timeDifference(now, n->lastReceiveTime);
//�����ʱ�ˣ��Ͽ�����
	  if (age > idleSeconds_)
      {
        conn->shutdown();
      }
	  //�������һ�㲻�ᷢ��
      else if (age < 0)
      {
        LOG_WARN << "Time jump";
        n->lastReceiveTime = now;
      }
      else//δ��ʱ
      {
        break;
      }
      ++it;
    }
	//˵�����ӹر�
    else
    {
      LOG_WARN << "Expired";
	  //�Ƴ�
      it = connectionList_.erase(it);
    }
  }
}

//��ӡ�����б�
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

