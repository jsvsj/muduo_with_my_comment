// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPSERVER_H
#define MUDUO_NET_TCPSERVER_H

#include <muduo/base/Types.h>
#include <muduo/net/TcpConnection.h>

#include <map>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{
namespace net
{

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

///
/// TCP server, supports single-threaded and thread-pool models.
///
/// This is an interface class, so don't expose too much details.
class TcpServer : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  //TcpServer(EventLoop* loop, const InetAddress& listenAddr);
  TcpServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg);
  ~TcpServer();  // force out-line dtor, for scoped_ptr members.

  const string& hostport() const { return hostport_; }
  const string& name() const { return name_; }

  /// Set the number of threads for handling input.
  ///
  /// Always accepts new connection in loop's thread.
  /// Must be called before @c start
  /// @param numThreads
  /// - 0 means all I/O in loop's thread, no thread will created.
  ///   this is the default value.
  /// - 1 means all I/O in another thread.
  /// - N means a thread pool with N threads, new connections
  ///   are assigned on a round-robin basis.
  void setThreadNum(int numThreads);

  
  void setThreadInitCallback(const ThreadInitCallback& cb)
  { threadInitCallback_ = cb; }

  /// Starts the server if it's not listenning.
  ///
  /// It's harmless to call it multiple times.
  /// Thread safe.
  void start();

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
  void newConnection(int sockfd, const InetAddress& peerAddr);

  /// Thread safe.
  //删除客户端连接
  void removeConnection(const TcpConnectionPtr& conn);
  
  /// Not thread safe, but in loop
  
  void removeConnectionInLoop(const TcpConnectionPtr& conn);

  //在callback中有 typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;

  typedef std::map<string, TcpConnectionPtr> ConnectionMap;

//acceptor所属的loop
  EventLoop* loop_;  // the acceptor loop

  const string hostport_;
  
  //server名字
  const string name_;

  //为调用accept函数
  boost::scoped_ptr<Acceptor> acceptor_; // avoid revealing Acceptor

  //线程池
  boost::scoped_ptr<EventLoopThreadPool> threadPool_;


  //WriteCompleteCallback类型在callback中定义
  
  //客户端有关连接状态改变的回调函数
  ConnectionCallback connectionCallback_;

  //接收到数据后描述符的回调函数
  MessageCallback messageCallback_;

  //写完数据后客户端描述符的回调函数
  WriteCompleteCallback writeCompleteCallback_;
  
  //线程执行的函数 
  ThreadInitCallback threadInitCallback_;
  
  //是否启动
  bool started_;
  
  // always in loop thread
	//下一个
  int nextConnId_;
  
  //存放客户端连接IO的map容器
  ConnectionMap connections_;
};

}
}

#endif  // MUDUO_NET_TCPSERVER_H
