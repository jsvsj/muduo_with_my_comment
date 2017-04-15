// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CONNECTOR_H
#define MUDUO_NET_CONNECTOR_H

#include <muduo/net/InetAddress.h>

#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;

class Connector : boost::noncopyable,
                  public boost::enable_shared_from_this<Connector>
{
 public:
  typedef boost::function<void (int sockfd)> NewConnectionCallback;

  Connector(EventLoop* loop, const InetAddress& serverAddr);
  ~Connector();

  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }

  void start();  // can be called in any thread
  void restart();  // must be called in loop thread
  void stop();  // can be called in any thread

  const InetAddress& serverAddress() const { return serverAddr_; }

 private:
 	//连接状态，分别为：未连接，正处于连接中，已经连接
  enum States { kDisconnected, kConnecting, kConnected };

  //下次重连是等待时间的最大值
  static const int kMaxRetryDelayMs = 30*1000;
  //下次重连是等待时间的初始值
  static const int kInitRetryDelayMs = 500;

  void setState(States s) { state_ = s; }
  void startInLoop();
  void stopInLoop();
  void connect();
  void connecting(int sockfd);
  void handleWrite();
  void handleError();
  void retry(int sockfd);
  int removeAndResetChannel();
  void resetChannel();

//所拥有的事件循环
  EventLoop* loop_;

  //服务器地址
  InetAddress serverAddr_;

  //是否调用了连接的有关函数,在调用stop断开连接时才会被改变
  bool connect_; // atomic

//连接状态
  States state_;  // FIXME: use atomic variable

//封装的channel,使用智能指针,所以能调用reset()
  boost::scoped_ptr<Channel> channel_;

//新建连接成功时的回调函数
  NewConnectionCallback newConnectionCallback_;

//重连延迟时间，毫秒
  int retryDelayMs_; 
};

}
}

#endif  // MUDUO_NET_CONNECTOR_H
