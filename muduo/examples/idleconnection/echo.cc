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

  //���ö�ʱ����ÿ��һ����,�ص�onTimer����
  loop->runEvery(1.0, boost::bind(&EchoServer::onTimer, this));

  connectionBuckets_.resize(idleSeconds);

  dumpConnectionBuckets();
}

void EchoServer::start()
{
  server_.start();
}


//����״̬�Ļص�����
void EchoServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "EchoServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

  if (conn->connected())
  {
  //���ӵ���,����һ��entry
    EntryPtr entry(new Entry(conn));//��ʱentry���ü���Ϊһ
    
    connectionBuckets_.back().insert(entry);//���뵽��β,entry���ü���Ϊ2

//��ӡ���ü���
	dumpConnectionBuckets();

//������
	WeakEntryPtr weakEntry(entry);

//�������ú�conn�����
	conn->setContext(weakEntry);

  //entry��ջ�ϣ����٣�entry���ü�����Ϊһ

  }
  else
  {
  //���ӶϿ�
    assert(!conn->getContext().empty());

  //��ù�����������
    WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));

//��ӡ���ü���
	LOG_DEBUG << "Entry use_count = " << weakEntry.use_count();
  }
}

//��Ϣ����
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

  //��ù�����������
  WeakEntryPtr weakEntry(boost::any_cast<WeakEntryPtr>(conn->getContext()));

//����������Ϊshare_ptr,��ʱentry���ü�����Ϊ2
  EntryPtr entry(weakEntry.lock());

  if (entry)
  {
  //���뵽β��
    connectionBuckets_.back().insert(entry);//���ü�����Ϊ3

//��ӡ���ü���
	dumpConnectionBuckets();
  }
  //���ü����ֱ�Ϊ2
}

//ÿ��һ����ִ�еĻص�����
void EchoServer::onTimer()
{
//�൱�ڽ�tailλ��ԭ�е�bucketɾ���ˣ���������һ���յ�bucket

//���д�ŵ�entry�����ü���ȫ��-1
  connectionBuckets_.push_back(Bucket());
//�����ü�����Ϊ0��entry���������������е���shutdown()�����Ͽ�����
	
//��ӡ���ü���
  dumpConnectionBuckets();
}

//�����λ������е����ӵ����ü�����ӡ
void EchoServer::dumpConnectionBuckets() const
{
  LOG_INFO << "size = " << connectionBuckets_.size();
  int idx = 0;
  //�������λ�����
  for (WeakConnectionList::const_iterator bucketI = connectionBuckets_.begin();
      bucketI != connectionBuckets_.end();
      ++bucketI, ++idx)
  {
  //�õ������е�set
    const Bucket& bucket = *bucketI;
    printf("[%d] len = %zd : ", idx, bucket.size());

//����set
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

