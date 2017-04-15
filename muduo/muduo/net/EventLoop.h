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

//��ʱ���к���
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
  //ȡ����ʱ
  void cancel(TimerId timerId);

  // internal usage
  //���̴߳�poll�л���
  void wakeup();

  //����channel
  void updateChannel(Channel* channel);

//�Ƴ�channel
  void removeChannel(Channel* channel);

  // pid_t threadId() const { return threadId_; }
  //�����ڱ��߳���
  void assertInLoopThread()
  {
    if (!isInLoopThread())
    {
      abortNotInLoopThread();
    }
  }
  
  //�Ƿ��ڸ��߳���
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  
  // bool callingPendingFunctors() const { return callingPendingFunctors_; }
  //�Ƿ����������¼��Ļص�������
  bool eventHandling() const { return eventHandling_; }

//��ȡ��ǰ�̵߳�eventloopָ��
  static EventLoop* getEventLoopOfCurrentThread();

 private:
  void abortNotInLoopThread();

  //��wakeup()���͵����ݵĶ��Ļص�����
  void handleRead();  // waked up

//�Ƿ��ڵ���doPendingFunctors()��
  void doPendingFunctors();

  void printActiveChannels() const; // DEBUG

  typedef std::vector<Channel*> ChannelList;

	//�Ƿ���loop��
  bool looping_; /* atomic *//

	//�Ƿ�Ҫ�Ƴ�loopѭ��
  bool quit_; /* atomic */

	//�Ƿ����������Ļص��¼���
  bool eventHandling_; /* atomic */

	//�Ƿ��ڵ������õĻص�������
  bool callingPendingFunctors_; /* atomic */

//��ѭ������ 
  int64_t iteration_;

  //���̵߳�id
  const pid_t threadId_;

  //poll�������ص�ʱ�� 
  Timestamp pollReturnTime_;

  //poll���󣬵���poll����
  boost::scoped_ptr<Poller> poller_;

  //��ʱ�¼��ļ���,ͨ��new����
  boost::scoped_ptr<TimerQueue> timerQueue_;

//����poll��������
  int wakeupFd_;
  // unlike in TimerQueue, which is an internal class,
  // we don't expose Channel to client.

  //�Ի��ѵ�������wakeupFd_�ķ�װ��Ϊ������������ûص�����
  boost::scoped_ptr<Channel> wakeupChannel_;

  //ͨ��poll�������صĻ�Ծ��IO��vector
  ChannelList activeChannels_;

  
  Channel* currentActiveChannel_;
  MutexLock mutex_;

  //ִ�еĻص������Ķ�̬����
  std::vector<Functor> pendingFunctors_; // @BuardedBy mutex_
};

}
}
#endif  // MUDUO_NET_EVENTLOOP_H
