// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/ThreadPool.h>

#include <muduo/base/Exception.h>

#include <boost/bind.hpp>
#include <assert.h>
#include <stdio.h>

using namespace muduo;

//线程池构造函数
ThreadPool::ThreadPool(const string& name)
  : mutex_(),
    cond_(mutex_),
    name_(name),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
//如果还没结束
  if (running_)
  {
    stop();
  }
}

//启动线程池，实质是根据线程个数创建若干个线程对象，然后启动每一个线程
void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;
  threads_.reserve(numThreads);
  //创建线程
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i);

	//创建线程对象,实质是初始化线程对象要执行的回调函数函数runInThread等，并且添加到vector中
    threads_.push_back(new muduo::Thread(
          boost::bind(&ThreadPool::runInThread, this), name_+id));
	//启动每一个线程
	threads_[i].start();
  }
}

//析构时调用
void ThreadPool::stop()
{
  {
  MutexLockGuard lock(mutex_);
  
  running_ = false;
  //唤醒所有的等待线程
  cond_.notifyAll();
  }
//对每个线程执行join函数,等待结束
  for_each(threads_.begin(),
           threads_.end(),
           boost::bind(&muduo::Thread::join, _1));
}

//添加任务
//往线程池中的任务队列添加任务回调函数
void ThreadPool::run(const Task& task)
{
//如果线程池中没有线程，则调用主线程执行任务
  if (threads_.empty())
  {
    task();
  }
  //将任务添加到queue中
  else
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(task);
	//通知任务不为空
    cond_.notify();
  }
}



//从任务队列中获取一个任务，相当于消费者
ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  //等待有任务或者结束标志
  while (queue_.empty() && running_)
  {
    cond_.wait();
  }
  Task task;
  if(!queue_.empty())
  {
    task = queue_.front();
    queue_.pop_front();
  }
  return task;
}


//每一个线程对象中执行的函数
void ThreadPool::runInThread()
{
  try
  {
  //调用stop()函数之后会将running_置为false
    while (running_)
    {
    //从任务队列中获取一个任务。然后执行
      Task task(take());
      if (task)
      {
      //执行任务回调函数
        task();
      }
    }
  }
  catch (const Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}

