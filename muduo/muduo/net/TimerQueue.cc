// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#define __STDC_LIMIT_MACROS
#include <muduo/net/TimerQueue.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Timer.h>
#include <muduo/net/TimerId.h>

#include <boost/bind.hpp>

#include <sys/timerfd.h>

namespace muduo
{
namespace net
{
namespace detail
{

int createTimerfd()
{
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                 TFD_NONBLOCK | TFD_CLOEXEC);
  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

//计算超时时刻与当前时刻的时间差
//when与当前时间差
struct timespec howMuchTimeFromNow(Timestamp when)
{
  int64_t microseconds = when.microSecondsSinceEpoch()
                         - Timestamp::now().microSecondsSinceEpoch();
  if (microseconds < 100)
  {
    microseconds = 100;
  }
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  return ts;
}

//可读时调用
void readTimerfd(int timerfd, Timestamp now)
{
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
  if (n != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
}

//重新设置timefd超时时间
void resetTimerfd(int timerfd, Timestamp expiration)
{
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  bzero(&newValue, sizeof newValue);
  bzero(&oldValue, sizeof oldValue);


  //计算超时时间与当前时间的差值 
  newValue.it_value = howMuchTimeFromNow(expiration);

  //设置超时时间
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
  if (ret)
  {
    LOG_SYSERR << "timerfd_settime()";
  }
}

}
}
}

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop* loop)
  : loop_(loop),
    timerfd_(createTimerfd()),	//初始化
    timerfdChannel_(loop, timerfd_),
    timers_(),
    callingExpiredTimers_(false)
{
//设置描述符可读时的回调函数
  timerfdChannel_.setReadCallback(
      boost::bind(&TimerQueue::handleRead, this));
  
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  //将timerfd添加到所属loop的poll的监听中，监听可读事件
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
  ::close(timerfd_);
  // do not remove channel, since we're in EventLoop::dtor();
  for (TimerList::iterator it = timers_.begin();
      it != timers_.end(); ++it)
  {
    delete it->second;
  }
}

//添加时间事件
TimerId TimerQueue::addTimer(const TimerCallback& cb,
                             Timestamp when,
                             double interval)
{
  Timer* timer = new Timer(cb, when, interval);
  //在对应的线程中调用
  loop_->runInLoop(
      boost::bind(&TimerQueue::addTimerInLoop, this, timer));
  return TimerId(timer, timer->sequence());
}

//取消时间对象
void TimerQueue::cancel(TimerId timerId)
{
  loop_->runInLoop(
      boost::bind(&TimerQueue::cancelInLoop, this, timerId));
}


//添加时间事件
void TimerQueue::addTimerInLoop(Timer* timer)
{
  loop_->assertInLoopThread();
  
  //插入，返回最早超时时间是否需要改变
  bool earliestChanged = insert(timer);

  if (earliestChanged)
  {
  	//重新设置超时时间
    resetTimerfd(timerfd_, timer->expiration());
  }
}


//取消时间对象
void TimerQueue::cancelInLoop(TimerId timerId)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());
  
									//序号
	//有 typedef std::pair<Timer*, int64_t> ActiveTimer;
  ActiveTimer timer(timerId.timer_, timerId.sequence_);
  
  ActiveTimerSet::iterator it = activeTimers_.find(timer);

	//如果找到
  if (it != activeTimers_.end())
  {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
    assert(n == 1); (void)n;
    delete it->first; // FIXME: no delete please
    activeTimers_.erase(it);
  }
  //如果正在处理时间对象的回调函数
  else if (callingExpiredTimers_)
  {
  	
    cancelingTimers_.insert(timer);
  }
  assert(timers_.size() == activeTimers_.size());
}

//定时描述符的可读的回调函数
void TimerQueue::handleRead()
{
  loop_->assertInLoopThread();
  Timestamp now(Timestamp::now());
  readTimerfd(timerfd_, now);

  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;
  
  cancelingTimers_.clear();
  // safe to callback outside critical section
  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {
  //调用时间对象的回调函数
    it->second->run();
  }

  callingExpiredTimers_ = false;

	//
  reset(expired, now);
}

//返回超时的定时器列表
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
  assert(timers_.size() == activeTimers_.size());
  
  //typedef std::pair<Timestamp, Timer*> Entry;
  std::vector<Entry> expired;

  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));

	//lower_bound() 返回指向大于（或等于）某值的第一个元素的迭代器
  TimerList::iterator end = timers_.lower_bound(sentry);
  assert(end == timers_.end() || now < end->first);

  std::copy(timers_.begin(), end, back_inserter(expired));
  timers_.erase(timers_.begin(), end);

  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1); (void)n;
  }

  assert(timers_.size() == activeTimers_.size());
  return expired;
}

//处理完时间回调函数后调用,就是在处理完一些超时的时间对象后，需要重新设置描述符的超时时间
void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
  Timestamp nextExpire;

  for (std::vector<Entry>::const_iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());

	//如果是重复，并且
    if (it->second->repeat()
        && cancelingTimers_.find(timer) == cancelingTimers_.end())
    {
      it->second->restart(now);
      insert(it->second);
    }
    else
    {
      // FIXME move to a free list
      delete it->second; // FIXME: no delete please
    }
  }

  if (!timers_.empty())
  {
  //获得最短的超时时间
    nextExpire = timers_.begin()->second->expiration();
  }

  if (nextExpire.valid())
  {
  //重新设置超时时间
    resetTimerfd(timerfd_, nextExpire);
  }
}

//插入一个时间对象
bool TimerQueue::insert(Timer* timer)
{
  loop_->assertInLoopThread();
  assert(timers_.size() == activeTimers_.size());

  //表示最先超时是否改变
  bool earliestChanged = false;

  //获取超时时间
  Timestamp when = timer->expiration();
  
  TimerList::iterator it = timers_.begin();

  //如果容器中还没有时间对象，或者容器中最早的超时时间要大于新添加到时间对象的超时时间
  //最早的超时时间就要改变，描述符的超时时间就需要设置
  if (it == timers_.end() || when < it->first)
  {
    earliestChanged = true;
  }
  
  {
  	//插入
    std::pair<TimerList::iterator, bool> result
      = timers_.insert(Entry(when, timer));
    assert(result.second); (void)result;
  }
  {
  	//插入
    std::pair<ActiveTimerSet::iterator, bool> result
      = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    assert(result.second); (void)result;
  }

  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}

