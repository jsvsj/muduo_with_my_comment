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
  //新建连接的回调函数
  void newConnection(int sockfd);
  
  /// Not thread safe, but in loop
  void removeConnection(const TcpConnectionPtr& conn);

//所拥有的loop对象，事件循环
  EventLoop* loop_;

//连接对象的智能指针，用于发起连接
  ConnectorPtr connector_; // avoid revealing Connector

//客户端名称
  const string name_;

//连接建立的回调函数
  ConnectionCallback connectionCallback_;

//消息到来的回调函数
  MessageCallback messageCallback_;

//消息发送完毕的回调函数
  WriteCompleteCallback writeCompleteCallback_;

//是否要重连，与connector中的重连的意义不同，指的是连接成功后又断开之后是否要重连
  bool retry_;   // atmoic

  //是否连接
  bool connect_; // atomic
  
  // always in loop thread
  //name+nextConnId标识一个连接
  int nextConnId_;
  
  mutable MutexLock mutex_;

  //连接成功后得到一个錞cpConnection对象
  TcpConnectionPtr connection_; // @BuardedBy mutex_
};

}
}

#endif  // MUDUO_NET_TCPCLIENT_H
