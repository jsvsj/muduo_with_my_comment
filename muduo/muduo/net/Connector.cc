// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/Connector.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>

using namespace muduo;
using namespace muduo::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
  : loop_(loop),
    serverAddr_(serverAddr),
    connect_(false),
    state_(kDisconnected),
    retryDelayMs_(kInitRetryDelayMs)
{
  LOG_DEBUG << "ctor[" << this << "]";
}

Connector::~Connector()
{
  LOG_DEBUG << "dtor[" << this << "]";
  assert(!channel_);
}

//发起连接,会调用startInLoop()
void Connector::start()
{
  connect_ = true;
  loop_->runInLoop(boost::bind(&Connector::startInLoop, this)); // FIXME: unsafe
}

//发起连接,会调用startInLoop()
void Connector::startInLoop()
{
  loop_->assertInLoopThread();
  assert(state_ == kDisconnected);
  if (connect_)
  {
  //调用连接函数
    connect();
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

//线程安全,停止连接，通过调用 stioInLoop(),
void Connector::stop()
{
  connect_ = false;
  loop_->runInLoop(boost::bind(&Connector::stopInLoop, this)); // FIXME: unsafe
  // FIXME: cancel timer
}

//停止连接
void Connector::stopInLoop()
{
  loop_->assertInLoopThread();
  //如果是正在连接,则停止，不再去连接
  if (state_ == kConnecting)
  {
    setState(kDisconnected);
    int sockfd = removeAndResetChannel();
//并非重连，此时设置了connect_ = false，只会调用retry()中的close()函数关闭连接
	retry(sockfd);
  }
}

//发起连接时被调用 
void Connector::connect()
{

//调用sockets名称空间中的函数，创建一个非阻塞的套接字
  int sockfd = sockets::createNonblockingOrDie();

  int ret = sockets::connect(sockfd, serverAddr_.getSockAddrInet());
  int savedErrno = (ret == 0) ? 0 : errno;
  switch (savedErrno)
  {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
		//连接成功，设置回调函数
      connecting(sockfd);
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
		//重连
      retry(sockfd);
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);
      break;

    default:
      LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);
      // connectErrorCallback_();
      break;
  }
}


//重启,将重连延迟时间重置,然后调用startInLoop()发起连接
void Connector::restart()
{
  loop_->assertInLoopThread();
  setState(kDisconnected);
  retryDelayMs_ = kInitRetryDelayMs;
  connect_ = true;
  startInLoop();
}

//连接后初始化channel
void Connector::connecting(int sockfd)
{
  setState(kConnecting);
  assert(!channel_);
  
  //初始化channel智能指针对象
  channel_.reset(new Channel(loop_, sockfd));

  //设置可写回调函数
  channel_->setWriteCallback(
      boost::bind(&Connector::handleWrite, this)); // FIXME: unsafe

//设置错误回调函数
  channel_->setErrorCallback(
      boost::bind(&Connector::handleError, this)); // FIXME: unsafe

  // channel_->tie(shared_from_this()); is not working,
  // as channel_ is not managed by shared_ptr
  
	//关注可写事件
  channel_->enableWriting();
}

//移除原有的channel设置的属性，并重置
int Connector::removeAndResetChannel()
{
  channel_->disableAll();
  channel_->remove();
  int sockfd = channel_->fd();
  // Can't reset channel_ here, because we are inside Channel::handleEvent
  loop_->queueInLoop(boost::bind(&Connector::resetChannel, this)); // FIXME: unsafe
  return sockfd;
}

//重置channel
void Connector::resetChannel()
{
//因为channel是智能指针对象,所以能调用这个方法
  channel_.reset();
}

//可写的回调函数,套接字应该随时处于可写状态
void Connector::handleWrite()
{
  LOG_TRACE << "Connector::handleWrite " << state_;

  if (state_ == kConnecting)
  {
  
  //将channal移除,重置,不再关注可写事件，防止一直触发
    int sockfd = removeAndResetChannel();
	
    int err = sockets::getSocketError(sockfd);
    if (err)
    {
      LOG_WARN << "Connector::handleWrite - SO_ERROR = "
               << err << " " << strerror_tl(err);
      retry(sockfd);
    }
    else if (sockets::isSelfConnect(sockfd))
    {
      LOG_WARN << "Connector::handleWrite - Self connect";
      retry(sockfd);
    }
    else
    {
    //设置状态为连接成功
      setState(kConnected);
      if (connect_)
      {
      //调用回调函数
        newConnectionCallback_(sockfd);
      }
      else
      {
        sockets::close(sockfd);
      }
    }
  }
  else
  {
    // what happened?
    assert(state_ == kDisconnected);
  }
}

//出错的回调函数
void Connector::handleError()
{
  LOG_ERROR << "Connector::handleError";
  assert(state_ == kConnecting);

  int sockfd = removeAndResetChannel();
  int err = sockets::getSocketError(sockfd);
  LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
  retry(sockfd);
}

//重连
void Connector::retry(int sockfd)
{
  sockets::close(sockfd);
  setState(kDisconnected);
  if (connect_)
  {
    LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
             << " in " << retryDelayMs_ << " milliseconds. ";

	//多长时间后重连
    loop_->runAfter(retryDelayMs_/1000.0,
                    boost::bind(&Connector::startInLoop, shared_from_this()));
	//改变下次重连等待的时间
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

