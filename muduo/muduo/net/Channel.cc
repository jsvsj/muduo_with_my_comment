// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd__)
  : loop_(loop),
    fd_(fd__),
    events_(0),
    revents_(0),
    index_(-1),
    logHup_(true),
    tied_(false),
    eventHandling_(false)
{
}

Channel::~Channel()
{
  assert(!eventHandling_);
}

//获得所属TcpConnection的弱引用
void Channel::tie(const boost::shared_ptr<void>& obj)
{
  tie_ = obj;
  tied_ = true;
}

//修改在poller中的该channal，如果poller中的有关集合中还为由此描述符，则添加到其中
//最终调用了所属loop中的poll对象的update函数
void Channel::update()
{
  loop_->updateChannel(this);
}

//删除在poller集合中于此相同的channal,即不在被poll监听
void Channel::remove()
{
  assert(isNoneEvent());
  loop_->removeChannel(this);
}

//处理事件，在pool循环中,活跃的描述符被遍历调用该各自的回调函数
void Channel::handleEvent(Timestamp receiveTime)
{
  boost::shared_ptr<void> guard;
  if (tied_)
  {
  //将弱引用提升,TcpConnection的对象的引用计数+1,变为2
    guard = tie_.lock();

	if (guard)
    {
      handleEventWithGuard(receiveTime);
    }
	//引用计数变为2
  }
  else
  {
    handleEventWithGuard(receiveTime);
  }
  //guard销毁，TcpConnection引用计数减一，变为1
}


//根据revents_调用不同的回调函数
void Channel::handleEventWithGuard(Timestamp receiveTime){

//表示正在调用该描述符的事件处理的回调函数
//在析构若该函数还在执行，则出错
  eventHandling_ = true;

//注意
//服务端主动关闭，客户端read()返回0，关闭连接,服务端会同时接受到POLLHUP和POLLIN
//而如果是客户端主动断开连接，则服务端只会接收到POLLIN

  if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
  {
    if (logHup_)
    {
      LOG_WARN << "Channel::handle_event() POLLHUP";
    }
    if (closeCallback_) closeCallback_();
  }

  if (revents_ & POLLNVAL)
  {
    LOG_WARN << "Channel::handle_event() POLLNVAL";
  }

  if (revents_ & (POLLERR | POLLNVAL))
  {
    if (errorCallback_) errorCallback_();
  }
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))
  {
    if (readCallback_) readCallback_(receiveTime);
  }
  if (revents_ & POLLOUT)
  {
    if (writeCallback_) writeCallback_();
  }
  eventHandling_ = false;
}

//打印，在loop中可输出日志
string Channel::reventsToString() const
{
  std::ostringstream oss;
  oss << fd_ << ": ";
  if (revents_ & POLLIN)
    oss << "IN ";
  if (revents_ & POLLPRI)
    oss << "PRI ";
  if (revents_ & POLLOUT)
    oss << "OUT ";
  if (revents_ & POLLHUP)
    oss << "HUP ";
  if (revents_ & POLLRDHUP)
    oss << "RDHUP ";
  if (revents_ & POLLERR)
    oss << "ERR ";
  if (revents_ & POLLNVAL)
    oss << "NVAL ";

  return oss.str().c_str();
}
