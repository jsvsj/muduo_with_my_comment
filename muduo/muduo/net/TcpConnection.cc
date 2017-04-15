// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpConnection.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Socket.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;


//默认的客户端有关连接状态改变的回调函数,在连接断开与建立是都会调用
void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)
{
//打印日志
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
}

//默认的客户端连接描述符的数据到达的回调函数
void muduo::net::defaultMessageCallback(const TcpConnectionPtr&,
                                        Buffer* buf,
                                        Timestamp)
{

//清空缓冲区
  buf->retrieveAll();
}


//构造函数 
TcpConnection::TcpConnection(EventLoop* loop,
                             const string& nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
  : loop_(CHECK_NOTNULL(loop)),
    name_(nameArg),
    state_(kConnecting),
    socket_(new Socket(sockfd)),
    channel_(new Channel(loop, sockfd)),
    localAddr_(localAddr),
    peerAddr_(peerAddr),
    highWaterMark_(64*1024*1024)
{
	//将回调函数设置到客户端描述符对应的channel_中,
	//这些函数有Tcpconnection定义

	//设置连接描述符可读的回调函数
  channel_->setReadCallback(
      boost::bind(&TcpConnection::handleRead, this, _1));

//设置连接描述符可写的回调函数
  channel_->setWriteCallback(
      boost::bind(&TcpConnection::handleWrite, this));

//设置连接断开的回调函数
  channel_->setCloseCallback(
      boost::bind(&TcpConnection::handleClose, this));

//发生错误的回调函数
  channel_->setErrorCallback(
      boost::bind(&TcpConnection::handleError, this));
  
  LOG_DEBUG << "TcpConnection::ctor[" <<  name_ << "] at " << this
            << " fd=" << sockfd;
  socket_->setKeepAlive(true);
}

//析构函数
TcpConnection::~TcpConnection()
{
  LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << " fd=" << channel_->fd();
}

//线程安全，可以跨线程调用
void TcpConnection::send(const void* data, size_t len)
{
  if (state_ == kConnected)
  {
	//如果当前线程就是在loop_所在线程,则调用sendInLoop();  
    if (loop_->isInLoopThread())
    {
      sendInLoop(data, len);
    }
    else
    {
      string message(static_cast<const char*>(data), len);
      loop_->runInLoop(
          boost::bind(&TcpConnection::sendInLoop,
                      this,     // FIXME
                      message));
    }
  }
}


void TcpConnection::send(const StringPiece& message)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(message);
    }
    else
    {
      loop_->runInLoop(
          boost::bind(&TcpConnection::sendInLoop,
                      this,     // FIXME
                      message.as_string()));
                    //std::forward<string>(message)));
    }
  }
}

// FIXME efficiency!!!
void TcpConnection::send(Buffer* buf)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(buf->peek(), buf->readableBytes());
      buf->retrieveAll();
    }
    else
    {
      loop_->runInLoop(
          boost::bind(&TcpConnection::sendInLoop,
                      this,     // FIXME
                      buf->retrieveAllAsString()));
                    //std::forward<string>(message)));
    }
  }
}

void TcpConnection::sendInLoop(const StringPiece& message)
{
  sendInLoop(message.data(), message.size());
}

//发送数据
void TcpConnection::sendInLoop(const void* data, size_t len)
{
  loop_->assertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool error = false;

  //如果处于未连接状态
  if (state_ == kDisconnected)
  {
    LOG_WARN << "disconnected, give up writing";
    return;
  }
  // if no thing in output queue, try writing directly
  
  //如果没有关注描述符的可写事件，并且缓冲区中没有遗留的要发送的数据
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
  {
  	//向描述符中写数据
    nwrote = sockets::write(channel_->fd(), data, len);
    if (nwrote >= 0)
    {
      remaining = len - nwrote;
	  //如果数据全部写完了
      if (remaining == 0 && writeCompleteCallback_)
      {
      
      //writeCompleteCallback_回调函数，在成员变量中设置
        loop_->queueInLoop(boost::bind(writeCompleteCallback_, shared_from_this()));
      }
    }
    else // nwrote < 0
    {
      nwrote = 0;
      if (errno != EWOULDBLOCK)
      {
        LOG_SYSERR << "TcpConnection::sendInLoop";
        if (errno == EPIPE) // FIXME: any others?
        {
          error = true;
        }
      }
    }
  }

  assert(remaining <= len);
  
  //如果数据没有一次发送完
  if (!error && remaining > 0)
  {
    LOG_TRACE << "I am going to write more data";
    size_t oldLen = outputBuffer_.readableBytes();

	if (oldLen + remaining >= highWaterMark_
        && oldLen < highWaterMark_
        && highWaterMarkCallback_)
    {
    //highWaterMarkCallback_回调函数，在成员变量中设置 
	 //缓冲区高水位标回调函数 
	 loop_->queueInLoop(boost::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
    }

	//将未写完的数据到缓冲区中
    outputBuffer_.append(static_cast<const char*>(data)+nwrote, remaining);

	if (!channel_->isWriting())
    {
    //关注描述符可写的事件
      channel_->enableWriting();
    }
  }
}

//关闭连接的客户端，不能直接关闭，要等到发送到客户端的数据发送完为止	
//如果数据没有发送完，只是将改变连接状态 
//写完之后检查状态，如若要关闭，则关闭
//关闭后,客户端read()返回0,断开连接，服务端会收到POLLHUP&POLLIN事件


//正常情况下，如果客户端主动关闭，服务端只会接受到POLLIN事件
void TcpConnection::shutdown()
{
  // FIXME: use compare and swap
  if (state_ == kConnected)
  {
  
  //设置状态
    setState(kDisconnecting);
    // FIXME: shared_from_this()?
    loop_->runInLoop(boost::bind(&TcpConnection::shutdownInLoop, this));
  }
}

//关闭连接的写
void TcpConnection::shutdownInLoop()
{
  loop_->assertInLoopThread();
  
  //如果没有监听描述符可写事件，即数据发送完了,就可以关闭
  if (!channel_->isWriting())
  {
    // we are not writing
    socket_->shutdownWrite();
  }
}

void TcpConnection::setTcpNoDelay(bool on)
{
  socket_->setTcpNoDelay(on);
}

//Tcpssever创建通过acceptor获取一个客户端描述符后回调这个函数，
//将描述符添加到poll中管理
void TcpConnection::connectEstablished()
{
  loop_->assertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);

  //在channal_中获得该对象的弱引用
  channel_->tie(shared_from_this());
  
  //将描述符添加到POLL中关注
  channel_->enableReading();

//回调函数,成员变量中设置
  connectionCallback_(shared_from_this());
//调用完成之后,形参销毁，TcpConnection引用计数减一，变为0，销毁
}

//客户端主动连接断开的回调函数
void TcpConnection::connectDestroyed()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected)
  {
    setState(kDisconnected);
    channel_->disableAll();
	
//连接状态的回调函数
    connectionCallback_(shared_from_this());
  }

  //移除对该描述符的监听
  channel_->remove();
}

//可读事件的回调函数
void TcpConnection::handleRead(Timestamp receiveTime)
{
  loop_->assertInLoopThread();
  int savedErrno = 0;
  
  //将数据读入到缓冲区中
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0)
  {
  //数据读完后的回调函数
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  }
  //如果客户端发送数据字节数为0，即客户端关闭,需要关闭连接
  //客户端主动断开连接
  else if (n == 0)
  {
    handleClose();
  }
  else
  {
    errno = savedErrno;
    LOG_SYSERR << "TcpConnection::handleRead";
    handleError();
  }
}

//可写的回调函数
void TcpConnection::handleWrite()
{
  loop_->assertInLoopThread();
  //如果关注了可写事件，说明上次有数据没有发送完
  if (channel_->isWriting())
  {
  //将缓冲区中的数据发送给客户端
    ssize_t n = sockets::write(channel_->fd(),
                               outputBuffer_.peek(),
                               outputBuffer_.readableBytes());
    if (n > 0)
    {
      outputBuffer_.retrieve(n);
      if (outputBuffer_.readableBytes() == 0)
      {
      	//取消对可写事件的关注
        channel_->disableWriting();
		
		//可写事件，调用成员变量中赋予的值
        if (writeCompleteCallback_)
        {
        
        //writeCompleteCallback_回调函数
          loop_->queueInLoop(boost::bind(writeCompleteCallback_, shared_from_this()));
        }
		//如果之前调用shutdownInLoop时因为数据没有发送完而没有关闭成功，则现在要再次关闭
        if (state_ == kDisconnecting)
        {
        //关闭连接
          shutdownInLoop();
        }
      }
      else
      {
        LOG_TRACE << "I am going to write more data";
      }
    }
    else
    {
      LOG_SYSERR << "TcpConnection::handleWrite";
      // if (state_ == kDisconnecting)
      // {
      //   shutdownInLoop();
      // }
    }
  }
  else
  {
    LOG_TRACE << "Connection fd = " << channel_->fd()
              << " is down, no more writing";
  }
}

//客户端主动断开连接时,关闭描述符的连接
void TcpConnection::handleClose()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "fd = " << channel_->fd() << " state = " << state_;
  assert(state_ == kConnected || state_ == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  setState(kDisconnected);

  //使文件描述符不再监听事件
  channel_->disableAll();

//TcpConnection引用计数变为3
  TcpConnectionPtr guardThis(shared_from_this());

  //成员变量中设置
  connectionCallback_(guardThis);
  
  // must be the last line
  //调用了TcpServer::removeConnection()函数
  closeCallback_(guardThis);

  //guardThis销毁，TcpConnection引用计数减一，变为2
}

void TcpConnection::handleError()
{
  int err = sockets::getSocketError(channel_->fd());
  LOG_ERROR << "TcpConnection::handleError [" << name_
            << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

