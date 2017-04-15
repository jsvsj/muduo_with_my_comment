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

//����poll����
Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
  // XXX pollfds_ shouldn't change //pollfd_  ����Ϊvector<struct pollfd>,

  //����ԭ����poll����
  int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);

  Timestamp now(Timestamp::now());
  if (numEvents > 0)
  {
    LOG_TRACE << numEvents << " events happended";
	//��pollfds�л�Ծ����������ӵ�activeChannels�У����ظ�loopѭ����
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


//����Ծ����������ӵ�activeChannels��
void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList* activeChannels) const
{
	//�������м������ļ�������
  for (PollFdList::const_iterator pfd = pollfds_.begin();
      pfd != pollfds_.end() && numEvents > 0; ++pfd)
  {
  	//����ǻ�Ծ���ļ�������
    if (pfd->revents > 0)
    {
      --numEvents;
      ChannelMap::const_iterator ch = channels_.find(pfd->fd);
      assert(ch != channels_.end());
      Channel* channel = ch->second;
      assert(channel->fd() == pfd->fd);
	  //���û�Ծ�¼�
      channel->set_revents(pfd->revents);
      // pfd->revents = 0;
      activeChannels->push_back(channel);
    }
  }
}


//����map������vector����
void PollPoller::updateChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();

	//�´�����channal��index��ʼ��Ϊ-1,��ʾΪһ���µĿͻ�������������,index��ס��channal
	//��vector�е�λ�ã�Ϊ���Ժ��޸�ʱ�ܿ����ҵ�
	//�� ����������ӵĿͻ���������������ӵ�������
  if (channel->index() < 0)
  {
    // a new one, add to pollfds_
    //��֤��map�洢�ĵ����Ҳ�������µ�������
    assert(channels_.find(channel->fd()) == channels_.end());

	//����һ��pollfd
    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;

	//��vector���������pollfd
    pollfds_.push_back(pfd);

	//������������Ӧ��channal�е�index�±�ֵ,Ϊvector�����һ��,Ϊ�˷���Ĳ���
    int idx = static_cast<int>(pollfds_.size())-1;
	channel->set_index(idx);

	//��map��������Ӹ�channal,��һ��Ԫ��Ϊ��channal����װ��������
    channels_[pfd.fd] = channel;
  }
	//������޸��Ѿ�����˵�������
  else
  {
    // update existing one
    //�������ҵ�
    assert(channels_.find(channel->fd()) != channels_.end());
	
    assert(channels_[channel->fd()] == channel);
	
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
	
    struct pollfd& pfd = pollfds_[idx];
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
	//�ж�������ٱ�����
    if (channel->isNoneEvent())
    {
      // ignore this pollfd
      pfd.fd = -channel->fd()-1;
    }
  }
}

//���״Ӽ�����������ļ�������
void PollPoller::removeChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd();
  
  //�������ҵ� 
  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);

  //���Ը��ļ����������ٱ�����
  assert(channel->isNoneEvent());
  
  int idx = channel->index();
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
  const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
  assert(pfd.fd == -channel->fd()-1 && pfd.events == channel->events());

  //������������map���Ƴ�
  size_t n = channels_.erase(channel->fd());
  
  assert(n == 1); (void)n;

  //�������������vector��ĩβ����ֱ���Ƴ�
  if (implicit_cast<size_t>(idx) == pollfds_.size()-1)
  {
    pollfds_.pop_back();
  }

  //���������������vector��ĩβ���򽫸���������ĩβ��������������֮���Ƴ����һ��������
  else
  {
  	//��ȡ���һ��Ԫ�ص��ļ�������
    int channelAtEnd = pollfds_.back().fd;
	//����
    iter_swap(pollfds_.begin()+idx, pollfds_.end()-1);
    if (channelAtEnd < 0)
    {
      channelAtEnd = -channelAtEnd-1;
    }
	//�޸�map�����һ����������������index
    channels_[channelAtEnd]->set_index(idx);
    pollfds_.pop_back();
  }
}

