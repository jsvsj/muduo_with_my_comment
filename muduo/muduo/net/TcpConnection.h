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

	//��ȡloop_
  EventLoop* getLoop() const { return loop_; }

  //��ȡ�ͻ��˵�����
  const string& name() const { return name_; }

  //��ȡ��ص�ַ����
  const InetAddress& localAddress() { return localAddr_; }
  const InetAddress& peerAddress() { return peerAddr_; }
  
  //�ж��Ƿ�ͻ����Ƿ�������״̬
  bool connected() const { return state_ == kConnected; }

//�������ݸ��ͻ���������
  // void send(string&& message); // C++11
  void send(const void* message, size_t len);
  void send(const StringPiece& message);
  // void send(Buffer&& message); // C++11
  void send(Buffer* message);  // this one will swap data

  //����������رտͻ��˵��������������ݷ������֮��������ر�
  void shutdown(); // NOT thread safe, no simultaneous calling

  //����������������
  void setTcpNoDelay(bool on);

  void setContext(const boost::any& context)
  { context_ = context; }

  const boost::any& getContext() const
  { return context_; }

  boost::any* getMutableContext()
  { return &context_; }

	//�����й�����״̬�Ļص�����
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

//�������������Ļص�����
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

//���ݷ�����ɵĻص�����
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

//��ˮλ��ص����� 
  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
  { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

	//��ȥ����������
  Buffer* inputBuffer()
  { return &inputBuffer_; }

  /// Internal use only.
  //���ÿͻ��������رպ�ʱ�Ļص�����
  void setCloseCallback(const CloseCallback& cb)
  { closeCallback_ = cb; }

  // called when TcpServer accepts a new connection
  
  //Tcpssever����ͨ��acceptor��ȡһ���ͻ�����������ص����������
  //����������ӵ�poll�й���
  void connectEstablished();   // should be called only once
  
  // called when TcpServer has removed me from its map
  //�ر�����ʱ����
  void connectDestroyed();  // should be called only once

 private:
 //�ͻ���������״̬ö��
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

//��Щ�����ᱻ����Ϊ��������Ӧ��channal�Ļص�����
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();
  
  //void sendInLoop(string&& message);
  void sendInLoop(const StringPiece& message);
  void sendInLoop(const void* message, size_t len);
  void shutdownInLoop();

  //��������״̬
  void setState(StateE s) { state_ = s; }

//������loop_
  EventLoop* loop_;
//�ͻ�������
  string name_;
  
  //�ͻ���������״̬����
  StateE state_;  // FIXME: use atomic variable
  // we don't expose those classes to client.
  
  //�Կͻ��˵��������ķ�װ
  boost::scoped_ptr<Socket> socket_;

  //�����������������ĸ����¼��Ļص������ķ�װ
  boost::scoped_ptr<Channel> channel_;

  
  InetAddress localAddr_;

  //�ͻ��˵�ַ��Ϣ��IP,�˿ں�
  InetAddress peerAddr_;

  //��Tcpserver�����ĸ��ص�����,����highWaterMarkCallback_����
  //�����й�����״̬�Ļص�����
  ConnectionCallback connectionCallback_;
  
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  HighWaterMarkCallback highWaterMarkCallback_;

  //�ͻ��������ر����Ӻ�Ļص�����
  CloseCallback closeCallback_;
  
  size_t highWaterMark_;

  //���ݻ������ķ�װ
  //Ӧ�ò���ջ�����
  Buffer inputBuffer_;

  //Ӧ�ò㷢�ͻ�����
  Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer.
  
  boost::any context_;//��һ��δ֪���͵������Ķ���
  // FIXME: creationTime_, lastReceiveTime_
  //        bytesReceived_, bytesSent_
  
};

//����TcpConnection������ָ��
typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;

}
}

#endif  // MUDUO_NET_TCPCONNECTION_H
