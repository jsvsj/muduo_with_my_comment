// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include <muduo/base/Mutex.h>
#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>

#include <boost/any.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
class TcpConnection : boost::noncopyable,
                      public boost::enable_shared_from_this<TcpConnection>
{
 public:
  /// Constructs a TcpConnection with a connected sockfd
  ///
  /// User should not create this object.
  TcpConnection(EventLoop* loop,
                const string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
  ~TcpConnection();

	//获取loop_
  EventLoop* getLoop() const { return loop_; }

  //获取客户端的名称
  const string& name() const { return name_; }

  //获取相关地址对象
  const InetAddress& localAddress() { return localAddr_; }
  const InetAddress& peerAddress() { return peerAddr_; }
  
  //判断是否客户端是否处于连接状态
  bool connected() const { return state_ == kConnected; }

//发送数据给客户端描述符
  // void send(string&& message); // C++11
  void send(const void* message, size_t len);
  void send(const StringPiece& message);
  // void send(Buffer&& message); // C++11
  void send(Buffer* message);  // this one will swap data

  //服务端主动关闭客户端的描述符，在数据发送完成之后才真正关闭
  void shutdown(); // NOT thread safe, no simultaneous calling

  //设置描述符的属性
  void setTcpNoDelay(bool on);

  void setContext(const boost::any& context)
  { context_ = context; }

  const boost::any& getContext() const
  { return context_; }

  boost::any* getMutableContext()
  { return &context_; }

	//设置有关连接状态的回调函数
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

//设置数据来到的回调函数
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

//数据发送完成的回调函数
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

//高水位标回调函数 
  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
  { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

	//回去缓冲区对象
  Buffer* inputBuffer()
  { return &inputBuffer_; }

  /// Internal use only.
  //设置客户端主动关闭后时的回调函数
  void setCloseCallback(const CloseCallback& cb)
  { closeCallback_ = cb; }

  // called when TcpServer accepts a new connection
  
  //Tcpssever创建通过acceptor获取一个客户端描述符后回调这个函数，
  //将描述符添加到poll中管理
  void connectEstablished();   // should be called only once
  
  // called when TcpServer has removed me from its map
  //关闭连接时调用
  void connectDestroyed();  // should be called only once

 private:
 //客户端所处的状态枚举
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

//这些函数会被设置为描述符对应的channal的回调函数
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();
  
  //void sendInLoop(string&& message);
  void sendInLoop(const StringPiece& message);
  void sendInLoop(const void* message, size_t len);
  void shutdownInLoop();

  //设置链接状态
  void setState(StateE s) { state_ = s; }

//所属的loop_
  EventLoop* loop_;
//客户端名称
  string name_;
  
  //客户端所处的状态变量
  StateE state_;  // FIXME: use atomic variable
  // we don't expose those classes to client.
  
  //对客户端的描述符的封装
  boost::scoped_ptr<Socket> socket_;

  //对描述符的描述符的各种事件的回调函数的封装
  boost::scoped_ptr<Channel> channel_;

  
  InetAddress localAddr_;

  //客户端地址信息，IP,端口号
  InetAddress peerAddr_;

  //被Tcpserver设置四个回调函数,其中highWaterMarkCallback_不是
  //设置有关连接状态的回调函数
  ConnectionCallback connectionCallback_;
  
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  HighWaterMarkCallback highWaterMarkCallback_;

  //客户端主动关闭连接后的回调函数
  CloseCallback closeCallback_;
  
  size_t highWaterMark_;

  //数据缓冲区的封装
  //应用层接收缓冲区
  Buffer inputBuffer_;

  //应用层发送缓冲区
  Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer.
  
  boost::any context_;//绑定一个未知类型的上下文对象
  // FIXME: creationTime_, lastReceiveTime_
  //        bytesReceived_, bytesSent_
  
};

//管理TcpConnection的智能指针
typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;

}
}

#endif  // MUDUO_NET_TCPCONNECTION_H
