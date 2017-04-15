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

    //new 一个Acceptor对象,在accepor的构造函数中会创建一个套接字,调用bind函数
    acceptor_(new Acceptor(loop, listenAddr)),

    //new 一个EventLoopThreadPool
    threadPool_(new EventLoopThreadPool(loop)),


//defaultConnectionCallback在TcpConnection.cc中有定义，全局函数
//连接状态的回调函数
    connectionCallback_(defaultConnectionCallback),

//defaultMessageCallback在TcpConnection.cc中有定义，全局函数,默认的处理数据到来的回调函数
    messageCallback_(defaultMessageCallback),
    
    started_(false),//是否调用start函数
    nextConnId_(1)  //下一个connid编号
{
	//socket可读的回调函数中调用的函数,
	//设置acceptor中的回调函数为newConnection,将新建的客户端的描述符添加到poll中
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

//设置线程池中的线程个数
void TcpServer::setThreadNum(int numThreads)
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

//启动server
void TcpServer::start()
{
  if (!started_)
  {
    started_ = true;
	//启动线程池,创建了几条线程
    threadPool_->start(threadInitCallback_);
  }

//在主线程中 将套接字变为被动,listen同时会把套接字添加到主线程的poll中监听可读事件 
  if (!acceptor_->listenning())
  {
    loop_->runInLoop(
        boost::bind(&Acceptor::listen, get_pointer(acceptor_)));
  }
}

//acceptor中接收到客户端后的回调函数中再次回调的函数
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{

	//参数sockfd为接收到的客户端的文件描述符
  loop_->assertInLoopThread();

	//从线程池中获取某个线程创建的EventLoop对象
	//即选取某个线程对连接的描述符进行监听,事件处理
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

  //新建一个客户端连接,用智能指针shared_ptr管理
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));

	//此时conn的引用计数为一
	
  //添加到客户端连接的智能指针map中

	//此时conn的引用计数变为2
  connections_[connName] = conn;

  //设置客户端的四个事件的回调函数,赋值给了TcpConnection,相对应的函数变量,最终赋值给了
  //描述符对应的channal的回调函数中回调的函数

  //设置有关连接的回调函数
  conn->setConnectionCallback(connectionCallback_);

  conn->setMessageCallback(messageCallback_);

  conn->setWriteCompleteCallback(writeCompleteCallback_);

//关闭连接的回调函数
  conn->setCloseCallback(
      boost::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe
      
  //自poll之后调用connectEstablished(),能将描述符添加到poll监管IO可读事件
  ioLoop->runInLoop(boost::bind(&TcpConnection::connectEstablished, conn));

	//conn变量销毁，new出的对象的引用计数有变为1
}

//客户端主动断开连接后的回调函数
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  // FIXME: unsafe
  loop_->runInLoop(boost::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

//客户端主动断开连接后的回调函数，移除map中的某个客户端连接
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();
  //在主线程的map中移除客户端连接
  //TcpConnection对象的引用计数减一变为2
  size_t n = connections_.erase(conn->name());
  (void)n;
  assert(n == 1);

  //获得conn所在的loop
  EventLoop* ioLoop = conn->getLoop();
  
  //在客户端描述符所属低io线程中调用
  ioLoop->queueInLoop(
  //TcpConnection的引用计数+1，变为3
      boost::bind(&TcpConnection::connectDestroyed, conn));
}

