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

//有事件到来时的回调函数，根据可读，可写来调用相应函数处理
  void handleEvent(Timestamp receiveTime);

  //给回调函数赋值
  //可读的回调函数
  void setReadCallback(const ReadEventCallback& cb)
  { readCallback_ = cb; }

  //可写的回调函数 
  void setWriteCallback(const EventCallback& cb)
  { writeCallback_ = cb; }

//关闭的回调函数
  void setCloseCallback(const EventCallback& cb)
  { closeCallback_ = cb; }

//错误的回调函数
  void setErrorCallback(const EventCallback& cb)
  { errorCallback_ = cb; }

  /// Tie this channel to the owner object managed by shared_ptr,
  /// prevent the owner object being destroyed in handleEvent.
  void tie(const boost::shared_ptr<void>&);

//返回文件描述符fd
  int fd() const { return fd_; }

  //返回监听的事件
  int events() const { return events_; }

  //设置活跃事件
  void set_revents(int revt) { revents_ = revt; } // used by pollers
  
  // int revents() const { return revents_; }
  //是否不再监听
  bool isNoneEvent() const { return events_ == kNoneEvent; }

//监听可读事件
  void enableReading() { events_ |= kReadEvent; update(); }
  
   void disableReading() { events_ &= ~kReadEvent; update(); }
  //监听可写事件
  void enableWriting() { events_ |= kWriteEvent; update(); }

//不再监听可写事件
  void disableWriting() { events_ &= ~kWriteEvent; update(); }

//不再监听所有事件,并调用了update()函数,
//最终调用了所属loop中的poll对象的update函数
  void disableAll() { events_ = kNoneEvent; update(); }

//判断是否监听可写事件
  bool isWriting() const { return events_ & kWriteEvent; }

  // for Poller
  //返回index
  int index() { return index_; }

//设置index
  void set_index(int idx) { index_ = idx; }

  // for debug
  //再loop中,可以被打印为日志
  string reventsToString() const;

  void doNotLogHup() { logHup_ = false; }

//返回所属的Eventloop
  EventLoop* ownerLoop() { return loop_; }


  void remove();

 private:
 	//最终调用了所属loop中的poll对象的update函数
  void update();
  
  void handleEventWithGuard(Timestamp receiveTime);

//代表不同事件的整数
  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

	//所属的loop
  EventLoop* loop_;

  //文件描述符
  const int  fd_;

  //关心的IO事件
  int        events_;

  //目前获活动的事件
  int        revents_;

  //在poll类中的下标，初始化为-1,记住该channal在监听的描述符的vector中的位置
  //方便查找
  int        index_; // used by Poller.

  
  bool       logHup_;

//保存弱引用
  boost::weak_ptr<void> tie_;

  //是否获得弱引用
  bool tied_;

  //是否正在回调函数中
  bool eventHandling_;

  //四个回调函数
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

}
}
#endif  // MUDUO_NET_CHANNEL_H
