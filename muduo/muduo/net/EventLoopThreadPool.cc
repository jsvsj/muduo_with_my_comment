// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThreadPool.h>

#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;


EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop)
  : baseLoop_(baseLoop),
    started_(false),
    numThreads_(0),
    next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
  // Don't delete loop, it's stack variable
}

//启动evenloop线程池,实质是根据线程数量创建若干个EventLoopThread对象，调用其startLoop()创建线程，loop
void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
  assert(!started_);
  baseLoop_->assertInLoopThread();

  started_ = true;

  for (int i = 0; i < numThreads_; ++i)
  {
  	//创建eventloop线程对象
    EventLoopThread* t = new EventLoopThread(cb);
	
	//存储到ptr_vector<EventLoopThread> threads_中，当threads_销毁时，
	//会自动销毁其中的指针所指的对象
    threads_.push_back(t);

	//存储线程中创建的EventLoop对象
    loops_.push_back(t->startLoop());
  }
  if (numThreads_ == 0 && cb)
  {
    cb(baseLoop_);
  }
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
  baseLoop_->assertInLoopThread();
  EventLoop* loop = baseLoop_;

//如果有多线程,则返回线程池中的某个loop，否则返回主线程中的loop
  if (!loops_.empty())
  {
    // round-robin
    loop = loops_[next_];
    ++next_;
    if (implicit_cast<size_t>(next_) >= loops_.size())
    {
      next_ = 0;
    }
  }
  return loop;
}

