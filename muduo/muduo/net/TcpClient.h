// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCLIENT_H
#define MUDUO_NET_TCPCLIENT_H

#include <boost/noncopyable.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/net/TcpConnection.h>

namespace muduo
{
namespace net
{

class Connector;
typedef boost::shared_ptr<Connector> ConnectorPtr;

class TcpClient : boost::noncopyable
{
 public:
  // TcpClient(EventLoop* loop);
  // TcpClient(EventLoop* loop, const string& host, uint16_t port);
  TcpClient(EventLoop* loop,
            const InetAddress& serverAddr,
            const string& name);
  ~TcpClient();  // force out-line dtor, for scoped_ptr members.

  void connect();
  void disconnect();
  void stop();

  TcpConnectionPtr connection() const
  {
    MutexLockGuard lock(mutex_);
    return connection_;
  }

  bool retry() const;
  void enableRetry() { retry_ = true; }

  /// Set connection callback.
  /// Not thread safe.
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  /// Set message callback.
  /// Not thread safe.
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  /// Set write complete callback.
  /// Not thread safe.
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

 private:
 	
  /// Not thread safe, but in loop
  //�½����ӵĻص�����
  void newConnection(int sockfd);
  
  /// Not thread safe, but in loop
  void removeConnection(const TcpConnectionPtr& conn);

//��ӵ�е�loop�����¼�ѭ��
  EventLoop* loop_;

//���Ӷ��������ָ�룬���ڷ�������
  ConnectorPtr connector_; // avoid revealing Connector

//�ͻ�������
  const string name_;

//���ӽ����Ļص�����
  ConnectionCallback connectionCallback_;

//��Ϣ�����Ļص�����
  MessageCallback messageCallback_;

//��Ϣ������ϵĻص�����
  WriteCompleteCallback writeCompleteCallback_;

//�Ƿ�Ҫ��������connector�е����������岻ͬ��ָ�������ӳɹ����ֶϿ�֮���Ƿ�Ҫ����
  bool retry_;   // atmoic

  //�Ƿ�����
  bool connect_; // atomic
  
  // always in loop thread
  //name+nextConnId��ʶһ������
  int nextConnId_;
  
  mutable MutexLock mutex_;

  //���ӳɹ���õ�һ���TcpConnection����
  TcpConnectionPtr connection_; // @BuardedBy mutex_
};

}
}

#endif  // MUDUO_NET_TCPCLIENT_H
