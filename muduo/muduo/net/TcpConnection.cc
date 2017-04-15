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


//Ĭ�ϵĿͻ����й�����״̬�ı�Ļص�����,�����ӶϿ��뽨���Ƕ������
void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)
{
//��ӡ��־
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
}

//Ĭ�ϵĿͻ������������������ݵ���Ļص�����
void muduo::net::defaultMessageCallback(const TcpConnectionPtr&,
                                        Buffer* buf,
                                        Timestamp)
{

//��ջ�����
  buf->retrieveAll();
}


//���캯�� 
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
	//���ص��������õ��ͻ�����������Ӧ��channel_��,
	//��Щ������Tcpconnection����

	//���������������ɶ��Ļص�����
  channel_->setReadCallback(
      boost::bind(&TcpConnection::handleRead, this, _1));

//����������������д�Ļص�����
  channel_->setWriteCallback(
      boost::bind(&TcpConnection::handleWrite, this));

//�������ӶϿ��Ļص�����
  channel_->setCloseCallback(
      boost::bind(&TcpConnection::handleClose, this));

//��������Ļص�����
  channel_->setErrorCallback(
      boost::bind(&TcpConnection::handleError, this));
  
  LOG_DEBUG << "TcpConnection::ctor[" <<  name_ << "] at " << this
            << " fd=" << sockfd;
  socket_->setKeepAlive(true);
}

//��������
TcpConnection::~TcpConnection()
{
  LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << " fd=" << channel_->fd();
}

//�̰߳�ȫ�����Կ��̵߳���
void TcpConnection::send(const void* data, size_t len)
{
  if (state_ == kConnected)
  {
	//�����ǰ�߳̾�����loop_�����߳�,�����sendInLoop();  
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

//��������
void TcpConnection::sendInLoop(const void* data, size_t len)
{
  loop_->assertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool error = false;

  //�������δ����״̬
  if (state_ == kDisconnected)
  {
    LOG_WARN << "disconnected, give up writing";
    return;
  }
  // if no thing in output queue, try writing directly
  
  //���û�й�ע�������Ŀ�д�¼������һ�������û��������Ҫ���͵�����
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
  {
  	//����������д����
    nwrote = sockets::write(channel_->fd(), data, len);
    if (nwrote >= 0)
    {
      remaining = len - nwrote;
	  //�������ȫ��д����
      if (remaining == 0 && writeCompleteCallback_)
      {
      
      //writeCompleteCallback_�ص��������ڳ�Ա����������
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
  
  //�������û��һ�η�����
  if (!error && remaining > 0)
  {
    LOG_TRACE << "I am going to write more data";
    size_t oldLen = outputBuffer_.readableBytes();

	if (oldLen + remaining >= highWaterMark_
        && oldLen < highWaterMark_
        && highWaterMarkCallback_)
    {
    //highWaterMarkCallback_�ص��������ڳ�Ա���������� 
	 //��������ˮλ��ص����� 
	 loop_->queueInLoop(boost::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
    }

	//��δд������ݵ���������
    outputBuffer_.append(static_cast<const char*>(data)+nwrote, remaining);

	if (!channel_->isWriting())
    {
    //��ע��������д���¼�
      channel_->enableWriting();
    }
  }
}

//�ر����ӵĿͻ��ˣ�����ֱ�ӹرգ�Ҫ�ȵ����͵��ͻ��˵����ݷ�����Ϊֹ	
//�������û�з����ֻ꣬�ǽ��ı�����״̬ 
//д��֮����״̬������Ҫ�رգ���ر�
//�رպ�,�ͻ���read()����0,�Ͽ����ӣ�����˻��յ�POLLHUP&POLLIN�¼�


//��������£�����ͻ��������رգ������ֻ����ܵ�POLLIN�¼�
void TcpConnection::shutdown()
{
  // FIXME: use compare and swap
  if (state_ == kConnected)
  {
  
  //����״̬
    setState(kDisconnecting);
    // FIXME: shared_from_this()?
    loop_->runInLoop(boost::bind(&TcpConnection::shutdownInLoop, this));
  }
}

//�ر����ӵ�д
void TcpConnection::shutdownInLoop()
{
  loop_->assertInLoopThread();
  
  //���û�м�����������д�¼��������ݷ�������,�Ϳ��Թر�
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

//Tcpssever����ͨ��acceptor��ȡһ���ͻ�����������ص����������
//����������ӵ�poll�й���
void TcpConnection::connectEstablished()
{
  loop_->assertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);

  //��channal_�л�øö����������
  channel_->tie(shared_from_this());
  
  //����������ӵ�POLL�й�ע
  channel_->enableReading();

//�ص�����,��Ա����������
  connectionCallback_(shared_from_this());
//�������֮��,�β����٣�TcpConnection���ü�����һ����Ϊ0������
}

//�ͻ����������ӶϿ��Ļص�����
void TcpConnection::connectDestroyed()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected)
  {
    setState(kDisconnected);
    channel_->disableAll();
	
//����״̬�Ļص�����
    connectionCallback_(shared_from_this());
  }

  //�Ƴ��Ը��������ļ���
  channel_->remove();
}

//�ɶ��¼��Ļص�����
void TcpConnection::handleRead(Timestamp receiveTime)
{
  loop_->assertInLoopThread();
  int savedErrno = 0;
  
  //�����ݶ��뵽��������
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0)
  {
  //���ݶ����Ļص�����
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  }
  //����ͻ��˷��������ֽ���Ϊ0�����ͻ��˹ر�,��Ҫ�ر�����
  //�ͻ��������Ͽ�����
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

//��д�Ļص�����
void TcpConnection::handleWrite()
{
  loop_->assertInLoopThread();
  //�����ע�˿�д�¼���˵���ϴ�������û�з�����
  if (channel_->isWriting())
  {
  //���������е����ݷ��͸��ͻ���
    ssize_t n = sockets::write(channel_->fd(),
                               outputBuffer_.peek(),
                               outputBuffer_.readableBytes());
    if (n > 0)
    {
      outputBuffer_.retrieve(n);
      if (outputBuffer_.readableBytes() == 0)
      {
      	//ȡ���Կ�д�¼��Ĺ�ע
        channel_->disableWriting();
		
		//��д�¼������ó�Ա�����и����ֵ
        if (writeCompleteCallback_)
        {
        
        //writeCompleteCallback_�ص�����
          loop_->queueInLoop(boost::bind(writeCompleteCallback_, shared_from_this()));
        }
		//���֮ǰ����shutdownInLoopʱ��Ϊ����û�з������û�йرճɹ���������Ҫ�ٴιر�
        if (state_ == kDisconnecting)
        {
        //�ر�����
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

//�ͻ��������Ͽ�����ʱ,�ر�������������
void TcpConnection::handleClose()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "fd = " << channel_->fd() << " state = " << state_;
  assert(state_ == kConnected || state_ == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  setState(kDisconnected);

  //ʹ�ļ����������ټ����¼�
  channel_->disableAll();

//TcpConnection���ü�����Ϊ3
  TcpConnectionPtr guardThis(shared_from_this());

  //��Ա����������
  connectionCallback_(guardThis);
  
  // must be the last line
  //������TcpServer::removeConnection()����
  closeCallback_(guardThis);

  //guardThis���٣�TcpConnection���ü�����һ����Ϊ2
}

void TcpConnection::handleError()
{
  int err = sockets::getSocketError(channel_->fd());
  LOG_ERROR << "TcpConnection::handleError [" << name_
            << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

