// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoop.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Singleton.h>
#include <muduo/net/Channel.h>
#include <muduo/net/Poller.h>
#include <muduo/net/SocketsOps.h>
#include <muduo/net/TimerQueue.h>

#include <boost/bind.hpp>

#include <signal.h>
#include <sys/eventfd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{
__thread EventLoop* t_loopInThisThread = 0;//保存this的值

const int kPollTimeMs = 10000;

//创建一个描述符, 用于唤醒
int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
 public:
  IgnoreSigPipe()
  {
    ::signal(SIGPIPE, SIG_IGN);
    LOG_TRACE << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

//忽略SIGPIPE信号，全局初始化是就会调用 
IgnoreSigPipe initObj;

}

//获取当前的loop
EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

//构造函数
EventLoop::EventLoop()
  : looping_(false),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    iteration_(0),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    //定时器集合,构造中会将timeefd添加到poll中监听可读事件,可读事件的回调函数
    //会调用超时的time的回调函数
    timerQueue_(new TimerQueue(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)),
    currentActiveChannel_(NULL)
{
  LOG_TRACE << "EventLoop created " << this << " in thread " << threadId_;
  //检查该线程中是否存在其他的EventLoop,一个线程中只能存在一个Eventloop
  if (t_loopInThisThread)
  {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  }
  else
  {
    t_loopInThisThread = this;
  }
  wakeupChannel_->setReadCallback(
      boost::bind(&EventLoop::handleRead, this));
  // we are always reading the wakeupfd
  //将wakeupfd设置到poll中监听
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
  ::close(wakeupFd_);
  t_loopInThisThread = NULL;
}

void EventLoop::loop()
{
  assert(!looping_);
  assertInLoopThread();
  looping_ = true;
  quit_ = false;
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_)
  {
    activeChannels_.clear();
	//activeChannels_为得到的活跃的描述符的抽象
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

	//循环次数
    ++iteration_;
	
	//根据日志级别打印日志
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels();
    }
    // TODO sort channel by priority
    
    //表示正在处理描述符对应的回调函数
    eventHandling_ = true;

	//遍历活跃的描述符的容器，执行每一个描述符的回调函数
    for (ChannelList::iterator it = activeChannels_.begin();
        it != activeChannels_.end(); ++it)
    {
      currentActiveChannel_ = *it;
	  //执行回调函数
      currentActiveChannel_->handleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = NULL;
	
	//表示结束了遍历处理动作
    eventHandling_ = false;

	//调用设置的其他函数
    doPendingFunctors();
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

void EventLoop::quit()
{
  quit_ = true;
  if (!isInLoopThread())
  {
    wakeup();
  }
}

void EventLoop::runInLoop(const Functor& cb)
{
  if (isInLoopThread())
  {
    cb();
  }
  else
  {
    queueInLoop(cb);
  }
}

void EventLoop::queueInLoop(const Functor& cb)
{
  {
  MutexLockGuard lock(mutex_);
  //添加到设置的回调函数动态数组中
  pendingFunctors_.push_back(cb);
  }

  if (!isInLoopThread() || callingPendingFunctors_)
  {
    wakeup();
  }
}


//定时执行执行某个动作，为了避免阻塞在poll上，倒置延误，使用了createTimerfd
//创建了一个描述符，并交给poll监控,设置该描述符的定时为所有定时器的最短时间
//通过loop最后的doPendingFunctors（）来设置定时器

TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb)
{
  return timerQueue_->addTimer(cb, time, 0.0);
}

TimerId EventLoop::runAfter(double delay, const TimerCallback& cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, cb);
}

TimerId EventLoop::runEvery(double interval, const TimerCallback& cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(cb, time, interval);
}

//取消定时器
void EventLoop::cancel(TimerId timerId)
{
  return timerQueue_->cancel(timerId);
}

//更新poller维护的相关集合中的描述符
void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

//删除poller维护的相关集合中的描述符
void EventLoop::removeChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

//如果不是在此线程中调用某个函数，则出错
void EventLoop::abortNotInLoopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}


//执行唤醒动作，往wakeupFd_描述符发送8个字节的数据，使poll能够返回,结束阻塞
void EventLoop::wakeup()
{
  uint64_t one = 1;
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

//唤醒的描述符可读的回调函数
void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

//在loop()循环中调用的函数
void EventLoop::doPendingFunctors()
{
  std::vector<Functor> functors;

  //表示loop_循环正在调用doPendingFunctors函数
  callingPendingFunctors_ = true;

  {
  MutexLockGuard lock(mutex_);
  functors.swap(pendingFunctors_);
  }
  
  //遍历调用pendingFunctors_中的函数
  for (size_t i = 0; i < functors.size(); ++i)
  {
    functors[i]();
  }
  //表示loop_循环结束调用doPendingFunctors函数
  callingPendingFunctors_ = false;
}

//打印活跃的描述符相关信息
void EventLoop::printActiveChannels() const
{
  for (ChannelList::const_iterator it = activeChannels_.begin();
      it != activeChannels_.end(); ++it)
  {
    const Channel* ch = *it;
    LOG_TRACE << "{" << ch->reventsToString() << "} ";
  }
}

