// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <muduo/base/Timestamp.h>

namespace muduo
{
namespace net
{

class EventLoop;

///
/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor.
/// The file descriptor could be a socket,
/// an eventfd, a timerfd, or a signalfd
class Channel : boost::noncopyable
{
 public:
  typedef boost::function<void()> EventCallback;
  typedef boost::function<void(Timestamp)> ReadEventCallback;

  Channel(EventLoop* loop, int fd);
  ~Channel();

//���¼�����ʱ�Ļص����������ݿɶ�����д��������Ӧ��������
  void handleEvent(Timestamp receiveTime);

  //���ص�������ֵ
  //�ɶ��Ļص�����
  void setReadCallback(const ReadEventCallback& cb)
  { readCallback_ = cb; }

  //��д�Ļص����� 
  void setWriteCallback(const EventCallback& cb)
  { writeCallback_ = cb; }

//�رյĻص�����
  void setCloseCallback(const EventCallback& cb)
  { closeCallback_ = cb; }

//����Ļص�����
  void setErrorCallback(const EventCallback& cb)
  { errorCallback_ = cb; }

  /// Tie this channel to the owner object managed by shared_ptr,
  /// prevent the owner object being destroyed in handleEvent.
  void tie(const boost::shared_ptr<void>&);

//�����ļ�������fd
  int fd() const { return fd_; }

  //���ؼ������¼�
  int events() const { return events_; }

  //���û�Ծ�¼�
  void set_revents(int revt) { revents_ = revt; } // used by pollers
  
  // int revents() const { return revents_; }
  //�Ƿ��ټ���
  bool isNoneEvent() const { return events_ == kNoneEvent; }

//�����ɶ��¼�
  void enableReading() { events_ |= kReadEvent; update(); }
  
   void disableReading() { events_ &= ~kReadEvent; update(); }
  //������д�¼�
  void enableWriting() { events_ |= kWriteEvent; update(); }

//���ټ�����д�¼�
  void disableWriting() { events_ &= ~kWriteEvent; update(); }

//���ټ��������¼�,��������update()����,
//���յ���������loop�е�poll�����update����
  void disableAll() { events_ = kNoneEvent; update(); }

//�ж��Ƿ������д�¼�
  bool isWriting() const { return events_ & kWriteEvent; }

  // for Poller
  //����index
  int index() { return index_; }

//����index
  void set_index(int idx) { index_ = idx; }

  // for debug
  //��loop��,���Ա���ӡΪ��־
  string reventsToString() const;

  void doNotLogHup() { logHup_ = false; }

//����������Eventloop
  EventLoop* ownerLoop() { return loop_; }


  void remove();

 private:
 	//���յ���������loop�е�poll�����update����
  void update();
  
  void handleEventWithGuard(Timestamp receiveTime);

//����ͬ�¼�������
  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

	//������loop
  EventLoop* loop_;

  //�ļ�������
  const int  fd_;

  //���ĵ�IO�¼�
  int        events_;

  //Ŀǰ�����¼�
  int        revents_;

  //��poll���е��±꣬��ʼ��Ϊ-1,��ס��channal�ڼ�������������vector�е�λ��
  //�������
  int        index_; // used by Poller.

  
  bool       logHup_;

//����������
  boost::weak_ptr<void> tie_;

  //�Ƿ���������
  bool tied_;

  //�Ƿ����ڻص�������
  bool eventHandling_;

  //�ĸ��ص�����
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

}
}
#endif  // MUDUO_NET_CHANNEL_H
