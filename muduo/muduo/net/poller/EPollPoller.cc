// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/poller/EPollPoller.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>

#include <boost/static_assert.hpp>

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>

using namespace muduo;
using namespace muduo::net;

// On Linux, the constants of poll(2) and epoll(4)
// are expected to be the same.
BOOST_STATIC_ASSERT(EPOLLIN == POLLIN);
BOOST_STATIC_ASSERT(EPOLLPRI == POLLPRI);
BOOST_STATIC_ASSERT(EPOLLOUT == POLLOUT);
BOOST_STATIC_ASSERT(EPOLLRDHUP == POLLRDHUP);
BOOST_STATIC_ASSERT(EPOLLERR == POLLERR);
BOOST_STATIC_ASSERT(EPOLLHUP == POLLHUP);

namespace
{
const int kNew = -1;
const int kAdded = 1;
const int kDeleted = 2;
}

//构造函数
EPollPoller::EPollPoller(EventLoop* loop)
  : Poller(loop),
  //初始化epollfd_,创建一个存储监听描述符的树形结构
    epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
    //为events_动态数组初始化容量
    events_(kInitEventListSize)
{
  if (epollfd_ < 0)
  {
    LOG_SYSFATAL << "EPollPoller::EPollPoller";
  }
}

EPollPoller::~EPollPoller()
{
	//采用RALL技法
  ::close(epollfd_);
}

//调用epoll_wait函数
Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
  int numEvents = ::epoll_wait(epollfd_,
                               &*events_.begin(),
                               static_cast<int>(events_.size()),
                               timeoutMs);
  Timestamp now(Timestamp::now());
  if (numEvents > 0)
  {
    LOG_TRACE << numEvents << " events happended";
	
	//将动态数组中的描述符添加到activeChannels中，供loop循环使用
    fillActiveChannels(numEvents, activeChannels);

	//如果返回的活跃的描述符的个数和动态数组的容量相同，则 容量*2
    if (implicit_cast<size_t>(numEvents) == events_.size())
    {
      events_.resize(events_.size()*2);
    }
  }
  else if (numEvents == 0)
  {
    LOG_TRACE << " nothing happended";
  }
  else
  {
    LOG_SYSERR << "EPollPoller::poll()";
  }
  //返回时间
  return now;
}

//将动态数组中的描述符添加到activeChannels中
void EPollPoller::fillActiveChannels(int numEvents,
                                     ChannelList* activeChannels) const
{
  assert(implicit_cast<size_t>(numEvents) <= events_.size());
  for (int i = 0; i < numEvents; ++i)
  {
    Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
#ifndef NDEBUG
    int fd = channel->fd();
    ChannelMap::const_iterator it = channels_.find(fd);
    assert(it != channels_.end());
    assert(it->second == channel);
#endif
    channel->set_revents(events_[i].events);
    activeChannels->push_back(channel);
  }
}

//将文件描述符修改或添加到监听结构中
void EPollPoller::updateChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();

  //获取channal的index
  const int index = channel->index();

  if (index == kNew || index == kDeleted)
  {
    // a new one, add with EPOLL_CTL_ADD
    int fd = channel->fd();

	 //如果是一个新的描述符，则添加
    if (index == kNew)
    {
      assert(channels_.find(fd) == channels_.end());

	  //将文件描述符添加到map集合中,下标为所对应的描述符
      channels_[fd] = channel;
    }
	
    else // index == kDeleted
    {
      assert(channels_.find(fd) != channels_.end());
      assert(channels_[fd] == channel);
    }
    channel->set_index(kAdded);
    update(EPOLL_CTL_ADD, channel);
  }
  else
  {
    // update existing one with EPOLL_CTL_MOD/DEL
    int fd = channel->fd();
    (void)fd;
    assert(channels_.find(fd) != channels_.end());
    assert(channels_[fd] == channel);
    assert(index == kAdded);
    if (channel->isNoneEvent())
    {
      update(EPOLL_CTL_DEL, channel);
      channel->set_index(kDeleted);
    }
    else
    {
      update(EPOLL_CTL_MOD, channel);
    }
  }
}

//删除监听的某个描述符
void EPollPoller::removeChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  
  //获取该channal对应的描述符
  int fd = channel->fd();
  
  LOG_TRACE << "fd = " << fd;
  assert(channels_.find(fd) != channels_.end());
  assert(channels_[fd] == channel);
  //断言该文件描述符没有要监听的事件
  assert(channel->isNoneEvent());
  
  int index = channel->index();
  assert(index == kAdded || index == kDeleted);
  
	//将该文件描述符从map中移除
  size_t n = channels_.erase(fd);
  (void)n;
  assert(n == 1);

  if (index == kAdded)
  {
    update(EPOLL_CTL_DEL, channel);
  }
  channel->set_index(kNew);
}

//根据选择对监听的描述符进行修改或添加
void EPollPoller::update(int operation, Channel* channel)
{
  struct epoll_event event;
  bzero(&event, sizeof event);
  event.events = channel->events();
  
  event.data.ptr = channel;
  int fd = channel->fd();
  
  if (::epoll_ctl(epollfd_, operation, fd, &event) < 0)
  {
    if (operation == EPOLL_CTL_DEL)
    {
      LOG_SYSERR << "epoll_ctl op=" << operation << " fd=" << fd;
    }
    else
    {
      LOG_SYSFATAL << "epoll_ctl op=" << operation << " fd=" << fd;
    }
  }
}





