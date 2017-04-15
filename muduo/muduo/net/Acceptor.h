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

//�ж��Ƿ��Ϊ�����׽���
  bool listenning() const { return listenning_; }
  void listen();

 private:
  void handleRead();

  //������eventloop
  EventLoop* loop_;

  //��װ���׽��ֵķ�װ
  Socket acceptSocket_;

  //���������׽��ֽ�һ����װ��channel,
  //������Ӽ����¼�����ӵ�poll�У�����channal����Ӧ�¼��Ļص�����
  Channel acceptChannel_;

  //�½����ӵĻص�����
  NewConnectionCallback newConnectionCallback_;

  //�׽����Ƿ������listen����Ϊ�����׽��� 
  bool listenning_;
  
  int idleFd_;
};

}
}

#endif  // MUDUO_NET_ACCEPTOR_H
