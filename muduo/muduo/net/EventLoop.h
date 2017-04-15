// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOP_H
#define MUDUO_NET_EVENTLOOP_H

#include <vector>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/TimerId.h>

namespace muduo
{
namespace net
{

class Channel;
class Poller;
class TimerQueue;

///
/// Reactor, at most one per thread.
///
/// This is an interface class, so don't expose too much details.
class EventLoop : boost::noncopyable
{
 public:
  typedef boost::function<void()> Functor;

  EventLoop();
  ~EventLoop();  // force out-line dtor, for scoped_ptr members.

  ///
  /// Loops forever.
  ///
  /// Must be called in the same thread as creation of the object.
  ///
  void loop();

  void quit();

  ///
  /// Time when poll returns, usually means data arrivial.
  ///
  Timestamp pollReturnTime() const { return pollReturnTime_; }

  int64_t iteration() const { return iteration_; }

  /// Runs callback immediately in the loop thread.
  /// It wakes up the loop, and run the cb.
  /// If in the same loop thread, cb is run within the function.
  /// Safe to call from other threads.
  void runInLoop(const Functor& cb);
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
  void queueInLoop(const Functor& cb);

  // timers

//定时运行函数
  ///
  /// Runs callback at 'time'.
  /// Safe to call from other threads.
  ///
  TimerId runAt(const Timestamp& time, const TimerCallback& cb);
  ///
  /// Runs callback after @c delay seconds.
  /// Safe to call from other threads.
  ///
  TimerId runAfter(double delay, const TimerCallback& cb);
  ///
  /// Runs callback every @c interval seconds.
  /// Safe to call from other threads.
  ///
  TimerId runEvery(double interval, const TimerCallback& cb);
  ///
  /// Cancels the timer.
  /// Safe to call from other threads.
  ///
  //取消定时
  void cancel(TimerId timerId);

  // internal usage
  //将线程从poll中唤醒
  void wakeup();

  //更新channel
  void updateChannel(Channel* channel);

//移除channel
  void removeChannel(Channel* channel);

  // pid_t threadId() const { return threadId_; }
  //断言在本线程中
  void assertInLoopThread()
  {
    if (!isInLoopThread())
    {
      abortNotInLoopThread();
    }
  }
  
  //是否在该线程中
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  
  // bool callingPendingFunctors() const { return callingPendingFunctors_; }
  //是否在描述符事件的回调函数中
  bool eventHandling() const { return eventHandling_; }

//获取当前线程的eventloop指针
  static EventLoop* getEventLoopOfCurrentThread();

 private:
  void abortNotInLoopThread();

  //对wakeup()发送的数据的读的回调函数
  void handleRead();  // waked up

//是否在调用doPendingFunctors()中
  void doPendingFunctors();

  void printActiveChannels() const; // DEBUG

  typedef std::vector<Channel*> ChannelList;

	//是否在loop中
  bool looping_; /* atomic *//

	//是否要推出loop循环
  bool quit_; /* atomic */

	//是否处于描述符的回调事件中
  bool eventHandling_; /* atomic */

	//是否处于调用设置的回调函数中
  bool callingPendingFunctors_; /* atomic */

//主循环次数 
  int64_t iteration_;

  //该线程的id
  const pid_t threadId_;

  //poll函数返回的时刻 
  Timestamp pollReturnTime_;

  //poll对象，调用poll函数
  boost::scoped_ptr<Poller> poller_;

  //定时事件的集合,通过new构造
  boost::scoped_ptr<TimerQueue> timerQueue_;

//唤醒poll的描述符
  int wakeupFd_;
  // unlike in TimerQueue, which is an internal class,
  // we don't expose Channel to client.

  //对唤醒的描述符wakeupFd_的封装，为其该描述符设置回调函数
  boost::scoped_ptr<Channel> wakeupChannel_;

  //通过poll函数返回的活跃的IO的vector
  ChannelList activeChannels_;

  
  Channel* currentActiveChannel_;
  MutexLock mutex_;

  //执行的回调函数的动态数组
  std::vector<Functor> pendingFunctors_; // @BuardedBy mutex_
};

}
}
#endif  // MUDUO_NET_EVENTLOOP_H
