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

//��������,�����startInLoop()
void Connector::start()
{
  connect_ = true;
  loop_->runInLoop(boost::bind(&Connector::startInLoop, this)); // FIXME: unsafe
}

//��������,�����startInLoop()
void Connector::startInLoop()
{
  loop_->assertInLoopThread();
  assert(state_ == kDisconnected);
  if (connect_)
  {
  //�������Ӻ���
    connect();
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

//�̰߳�ȫ,ֹͣ���ӣ�ͨ������ stioInLoop(),
void Connector::stop()
{
  connect_ = false;
  loop_->runInLoop(boost::bind(&Connector::stopInLoop, this)); // FIXME: unsafe
  // FIXME: cancel timer
}

//ֹͣ����
void Connector::stopInLoop()
{
  loop_->assertInLoopThread();
  //�������������,��ֹͣ������ȥ����
  if (state_ == kConnecting)
  {
    setState(kDisconnected);
    int sockfd = removeAndResetChannel();
//������������ʱ������connect_ = false��ֻ�����retry()�е�close()�����ر�����
	retry(sockfd);
  }
}

//��������ʱ������ 
void Connector::connect()
{

//����sockets���ƿռ��еĺ���������һ�����������׽���
  int sockfd = sockets::createNonblockingOrDie();

  int ret = sockets::connect(sockfd, serverAddr_.getSockAddrInet());
  int savedErrno = (ret == 0) ? 0 : errno;
  switch (savedErrno)
  {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
		//���ӳɹ������ûص�����
      connecting(sockfd);
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
		//����
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


//����,�������ӳ�ʱ������,Ȼ�����startInLoop()��������
void Connector::restart()
{
  loop_->assertInLoopThread();
  setState(kDisconnected);
  retryDelayMs_ = kInitRetryDelayMs;
  connect_ = true;
  startInLoop();
}

//���Ӻ��ʼ��channel
void Connector::connecting(int sockfd)
{
  setState(kConnecting);
  assert(!channel_);
  
  //��ʼ��channel����ָ�����
  channel_.reset(new Channel(loop_, sockfd));

  //���ÿ�д�ص�����
  channel_->setWriteCallback(
      boost::bind(&Connector::handleWrite, this)); // FIXME: unsafe

//���ô���ص�����
  channel_->setErrorCallback(
      boost::bind(&Connector::handleError, this)); // FIXME: unsafe

  // channel_->tie(shared_from_this()); is not working,
  // as channel_ is not managed by shared_ptr
  
	//��ע��д�¼�
  channel_->enableWriting();
}

//�Ƴ�ԭ�е�channel���õ����ԣ�������
int Connector::removeAndResetChannel()
{
  channel_->disableAll();
  channel_->remove();
  int sockfd = channel_->fd();
  // Can't reset channel_ here, because we are inside Channel::handleEvent
  loop_->queueInLoop(boost::bind(&Connector::resetChannel, this)); // FIXME: unsafe
  return sockfd;
}

//����channel
void Connector::resetChannel()
{
//��Ϊchannel������ָ�����,�����ܵ����������
  channel_.reset();
}

//��д�Ļص�����,�׽���Ӧ����ʱ���ڿ�д״̬
void Connector::handleWrite()
{
  LOG_TRACE << "Connector::handleWrite " << state_;

  if (state_ == kConnecting)
  {
  
  //��channal�Ƴ�,����,���ٹ�ע��д�¼�����ֹһֱ����
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
    //����״̬Ϊ���ӳɹ�
      setState(kConnected);
      if (connect_)
      {
      //���ûص�����
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

//����Ļص�����
void Connector::handleError()
{
  LOG_ERROR << "Connector::handleError";
  assert(state_ == kConnecting);

  int sockfd = removeAndResetChannel();
  int err = sockets::getSocketError(sockfd);
  LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
  retry(sockfd);
}

//����
void Connector::retry(int sockfd)
{
  sockets::close(sockfd);
  setState(kDisconnected);
  if (connect_)
  {
    LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
             << " in " << retryDelayMs_ << " milliseconds. ";

	//�೤ʱ�������
    loop_->runAfter(retryDelayMs_/1000.0,
                    boost::bind(&Connector::startInLoop, shared_from_this()));
	//�ı��´������ȴ���ʱ��
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

