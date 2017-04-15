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


//���̷߳���˵��Ż�
class ChatServer : boost::noncopyable
{
 public:
  ChatServer(EventLoop* loop,
             const InetAddress& listenAddr)
  : loop_(loop),
    server_(loop, listenAddr, "ChatServer"),
    codec_(boost::bind(&ChatServer::onStringMessage, this, _1, _2, _3)),
	//��ʱ����ָ�������ü���Ϊ1	
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
 	//����״̬�Ļص����������ڿͻ���������Ͽ����ᱻ����
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");

    MutexLockGuard lock(mutex_);

	//������ü���>1
    if (!connections_.unique())
    {
    //������һ��ConnectionList
    //ִ������һ�д����ԭ�����б��������ü���-1,new�����µ��б��������ü���Ϊ1
      connections_.reset(new ConnectionList(*connections_));
    }
    assert(connections_.unique());

//�ڸ������޸ģ�����Ӱ��ԭ���б����Զ����ڶ���ʱ����Ҫmutex
//ԭ�����б�����ڶ���������֮�����ü������Ϊ0���Զ����٣����������ԭ�����б����
//���ԭ�������ü�������һ,��û���߳̽��ж��������򲻻�ִ������if����е����ݣ��������������������ͻ���ԭ�������޸ģ�
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

//��Ϣ�����Ļص�����
//�ڶ��˶�ʱ���Ƚ����ü���+1���������-1
  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
  //���ü���+1,mutex�����ط�Χ��С��
    ConnectionListPtr connections = getConnectionList();
  //���ܵ�mutex�������ͻ����б�д�Ÿı�����ô��
  //ʵ���ϣ�д��������һ���������޸ģ��������赣��
    for (ConnectionList::iterator it = connections->begin();
        it != connections->end();
        ++it)
    {
    //ת����Ϣ
    
    //��Ϣ�����һ���ͻ��������һ���ͻ���֮����ӳ����ɱȽϴ�
      codec_.send(get_pointer(*it), message);
    }

	//ջ�ϵ�connections���٣����ü���-1
  }

//��ȡ�ͻ����б�ָ��
  ConnectionListPtr getConnectionList()
  {
  //����
    MutexLockGuard lock(mutex_);
    return connections_;
  }

  EventLoop* loop_;
  TcpServer server_;
  LengthHeaderCodec codec_;
  MutexLock mutex_;
  
  //std::set<TcpConnectionPtr> ConnectionList��shared_ptr����ָ��
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

