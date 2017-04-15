// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpServer.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Acceptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const string& nameArg)
  : loop_(CHECK_NOTNULL(loop)),
    hostport_(listenAddr.toIpPort()),
    name_(nameArg),

    //new һ��Acceptor����,��accepor�Ĺ��캯���лᴴ��һ���׽���,����bind����
    acceptor_(new Acceptor(loop, listenAddr)),

    //new һ��EventLoopThreadPool
    threadPool_(new EventLoopThreadPool(loop)),


//defaultConnectionCallback��TcpConnection.cc���ж��壬ȫ�ֺ���
//����״̬�Ļص�����
    connectionCallback_(defaultConnectionCallback),

//defaultMessageCallback��TcpConnection.cc���ж��壬ȫ�ֺ���,Ĭ�ϵĴ������ݵ����Ļص�����
    messageCallback_(defaultMessageCallback),
    
    started_(false),//�Ƿ����start����
    nextConnId_(1)  //��һ��connid���
{
	//socket�ɶ��Ļص������е��õĺ���,
	//����acceptor�еĻص�����ΪnewConnection,���½��Ŀͻ��˵���������ӵ�poll��
  acceptor_->setNewConnectionCallback(
      boost::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

  for (ConnectionMap::iterator it(connections_.begin());
      it != connections_.end(); ++it)
  {
    TcpConnectionPtr conn = it->second;
    it->second.reset();
    conn->getLoop()->runInLoop(
      boost::bind(&TcpConnection::connectDestroyed, conn));
    conn.reset();
  }
}

//�����̳߳��е��̸߳���
void TcpServer::setThreadNum(int numThreads)
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

//����server
void TcpServer::start()
{
  if (!started_)
  {
    started_ = true;
	//�����̳߳�,�����˼����߳�
    threadPool_->start(threadInitCallback_);
  }

//�����߳��� ���׽��ֱ�Ϊ����,listenͬʱ����׽�����ӵ����̵߳�poll�м����ɶ��¼� 
  if (!acceptor_->listenning())
  {
    loop_->runInLoop(
        boost::bind(&Acceptor::listen, get_pointer(acceptor_)));
  }
}

//acceptor�н��յ��ͻ��˺�Ļص��������ٴλص��ĺ���
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{

	//����sockfdΪ���յ��Ŀͻ��˵��ļ�������
  loop_->assertInLoopThread();

	//���̳߳��л�ȡĳ���̴߳�����EventLoop����
	//��ѡȡĳ���̶߳����ӵ����������м���,�¼�����
  EventLoop* ioLoop = threadPool_->getNextLoop();
  char buf[32];
  snprintf(buf, sizeof buf, ":%s#%d", hostport_.c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary

  //�½�һ���ͻ�������,������ָ��shared_ptr����
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));

	//��ʱconn�����ü���Ϊһ
	
  //��ӵ��ͻ������ӵ�����ָ��map��

	//��ʱconn�����ü�����Ϊ2
  connections_[connName] = conn;

  //���ÿͻ��˵��ĸ��¼��Ļص�����,��ֵ����TcpConnection,���Ӧ�ĺ�������,���ո�ֵ����
  //��������Ӧ��channal�Ļص������лص��ĺ���

  //�����й����ӵĻص�����
  conn->setConnectionCallback(connectionCallback_);

  conn->setMessageCallback(messageCallback_);

  conn->setWriteCompleteCallback(writeCompleteCallback_);

//�ر����ӵĻص�����
  conn->setCloseCallback(
      boost::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe
      
  //��poll֮�����connectEstablished(),�ܽ���������ӵ�poll���IO�ɶ��¼�
  ioLoop->runInLoop(boost::bind(&TcpConnection::connectEstablished, conn));

	//conn�������٣�new���Ķ�������ü����б�Ϊ1
}

//�ͻ��������Ͽ����Ӻ�Ļص�����
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  // FIXME: unsafe
  loop_->runInLoop(boost::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

//�ͻ��������Ͽ����Ӻ�Ļص��������Ƴ�map�е�ĳ���ͻ�������
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();
  //�����̵߳�map���Ƴ��ͻ�������
  //TcpConnection��������ü�����һ��Ϊ2
  size_t n = connections_.erase(conn->name());
  (void)n;
  assert(n == 1);

  //���conn���ڵ�loop
  EventLoop* ioLoop = conn->getLoop();
  
  //�ڿͻ���������������io�߳��е���
  ioLoop->queueInLoop(
  //TcpConnection�����ü���+1����Ϊ3
      boost::bind(&TcpConnection::connectDestroyed, conn));
}

