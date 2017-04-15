// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThread.h>

#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;


//构造函数
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb)
  : loop_(NULL),
    exiting_(false),
    //创建线程
    thread_(boost::bind(&EventLoopThread::threadFunc, this)),
    mutex_(),
    cond_(mutex_),
    callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  loop_->quit();
  thread_.join();
}

//启动线程，
EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  thread_.start();

  {
    MutexLockGuard lock(mutex_);
	//等待线程创建EventLoop对象
    while (loop_ == NULL)
    {
      cond_.wait();
    }
  }

//返回线程中创建的loop
  return loop_;
}

//线程中执行的函数
void EventLoopThread::threadFunc()
{
//在线程中创建一个loop
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);
  }

  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;
    cond_.notify();
  }

  loop.loop();
  //assert(exiting_);
}

