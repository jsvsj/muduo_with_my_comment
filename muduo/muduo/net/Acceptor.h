// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include <muduo/net/Channel.h>
#include <muduo/net/Socket.h>

namespace muduo
{
namespace net
{

class EventLoop;
class InetAddress;

///
/// Acceptor of incoming TCP connections.
///
class Acceptor : boost::noncopyable
{
 public:
  typedef boost::function<void (int sockfd,
                                const InetAddress&)> NewConnectionCallback;

  Acceptor(EventLoop* loop, const InetAddress& listenAddr);
  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }

//判断是否变为被动套接字
  bool listenning() const { return listenning_; }
  void listen();

 private:
  void handleRead();

  //所属的eventloop
  EventLoop* loop_;

  //封装的套接字的封装
  Socket acceptSocket_;

  //将创建的套接字进一步封装成channel,
  //即会添加监听事件，添加到poll中，设置channal的相应事件的回调函数
  Channel acceptChannel_;

  //新建连接的回调函数
  NewConnectionCallback newConnectionCallback_;

  //套接字是否调用了listen，变为被动套接字 
  bool listenning_;
  
  int idleFd_;
};

}
}

#endif  // MUDUO_NET_ACCEPTOR_H
