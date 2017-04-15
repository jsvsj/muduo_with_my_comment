// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Acceptor.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>

using namespace muduo;
using namespace muduo::net;

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr)
  : loop_(loop),
  //初始化一个套接字
    acceptSocket_(sockets::createNonblockingOrDie()),
    
    acceptChannel_(loop, acceptSocket_.fd()),
    listenning_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true);
  acceptSocket_.bindAddress(listenAddr);
  
  //设置channel的回调函数,即套接字可读时的回调函数
  acceptChannel_.setReadCallback(
      boost::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
  acceptChannel_.disableAll();
  acceptChannel_.remove();
  ::close(idleFd_);
}

//将套接字设置为监听
void Acceptor::listen()
{
  loop_->assertInLoopThread();
  listenning_ = true;
  acceptSocket_.listen();
  
  //将文件描述符添加到Pool所维持的vector中,监听可读事件
  acceptChannel_.enableReading();
}

//监听描述符的可读时的回调函数
void Acceptor::handleRead()
{
  loop_->assertInLoopThread();

  //初始化一个客户端的地址
  InetAddress peerAddr(0);
  //FIXME loop until no more

  //接收客户端连接
  int connfd = acceptSocket_.accept(&peerAddr);
  
  if (connfd >= 0)
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (newConnectionCallback_)
    {
    //调用给定的回调函数,在Tcpserver中可设置,
    //将新建的客户端的描述符添加到poll中监管
      newConnectionCallback_(connfd, peerAddr);
    }
    else
    {
      sockets::close(connfd);
    }
  }
  else
  {
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of livev.
    if (errno == EMFILE)
    {
      ::close(idleFd_);
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    }
  }
}

