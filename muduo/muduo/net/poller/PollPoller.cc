// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/poller/PollPoller.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <muduo/net/Channel.h>

#include <assert.h>
#include <poll.h>

using namespace muduo;
using namespace muduo::net;

PollPoller::PollPoller(EventLoop* loop)
  : Poller(loop)
{
}

PollPoller::~PollPoller()
{
}

//调用poll函数
Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
  // XXX pollfds_ shouldn't change //pollfd_  类型为vector<struct pollfd>,

  //调用原生的poll函数
  int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);

  Timestamp now(Timestamp::now());
  if (numEvents > 0)
  {
    LOG_TRACE << numEvents << " events happended";
	//将pollfds中活跃的描述符添加到activeChannels中，返回给loop循环中
    fillActiveChannels(numEvents, activeChannels);
  }
  else if (numEvents == 0)
  {
    LOG_TRACE << " nothing happended";
  }
  else
  {
    LOG_SYSERR << "PollPoller::poll()";
  }
  return now;
}


//将活跃的描述符添加到activeChannels中
void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList* activeChannels) const
{
	//遍历所有监听的文件描述符
  for (PollFdList::const_iterator pfd = pollfds_.begin();
      pfd != pollfds_.end() && numEvents > 0; ++pfd)
  {
  	//如果是活跃的文件描述符
    if (pfd->revents > 0)
    {
      --numEvents;
      ChannelMap::const_iterator ch = channels_.find(pfd->fd);
      assert(ch != channels_.end());
      Channel* channel = ch->second;
      assert(channel->fd() == pfd->fd);
	  //设置活跃事件
      channel->set_revents(pfd->revents);
      // pfd->revents = 0;
      activeChannels->push_back(channel);
    }
  }
}


//更新map集合于vector集合
void PollPoller::updateChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();

	//新创建的channal中index初始化为-1,表示为一个新的客户端连接描述符,index记住该channal
	//在vector中的位置，为了以后修改时能快速找到
	//即 如果是新连接的客户端描述符，则添加到集合中
  if (channel->index() < 0)
  {
    // a new one, add to pollfds_
    //保证在map存储的当中找不到这个新的描述符
    assert(channels_.find(channel->fd()) == channels_.end());

	//创建一个pollfd
    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;

	//往vector集合中添加pollfd
    pollfds_.push_back(pfd);

	//设置描述符对应的channal中的index下标值,为vector的最后一个,为了方便的查找
    int idx = static_cast<int>(pollfds_.size())-1;
	channel->set_index(idx);

	//往map集合中添加该channal,第一个元素为该channal所封装的描述符
    channels_[pfd.fd] = channel;
  }
	//如果是修改已经添加了的描述符
  else
  {
    // update existing one
    //断言能找到
    assert(channels_.find(channel->fd()) != channels_.end());
	
    assert(channels_[channel->fd()] == channel);
	
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
	
    struct pollfd& pfd = pollfds_[idx];
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
	//判断如果不再被监听
    if (channel->isNoneEvent())
    {
      // ignore this pollfd
      pfd.fd = -channel->fd()-1;
    }
  }
}

//彻底从集合中清除该文件描述符
void PollPoller::removeChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd();
  
  //断言能找到 
  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);

  //断言该文件描述符不再被监听
  assert(channel->isNoneEvent());
  
  int idx = channel->index();
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
  const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
  assert(pfd.fd == -channel->fd()-1 && pfd.events == channel->events());

  //将该描述符从map中移除
  size_t n = channels_.erase(channel->fd());
  
  assert(n == 1); (void)n;

  //如果该描述符在vector的末尾，则直接移除
  if (implicit_cast<size_t>(idx) == pollfds_.size()-1)
  {
    pollfds_.pop_back();
  }

  //如果该描述符不再vector的末尾，则将该描述符与末尾的描述符交换，之后移除最后一个描述符
  else
  {
  	//获取最后一个元素的文件描述符
    int channelAtEnd = pollfds_.back().fd;
	//交换
    iter_swap(pollfds_.begin()+idx, pollfds_.end()-1);
    if (channelAtEnd < 0)
    {
      channelAtEnd = -channelAtEnd-1;
    }
	//修改map中最后一个被交换的描述符index
    channels_[channelAtEnd]->set_index(idx);
    pollfds_.pop_back();
  }
}

