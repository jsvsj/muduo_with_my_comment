// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADPOOL_H
#define MUDUO_BASE_THREADPOOL_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>
#include <muduo/base/Types.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <deque>

namespace muduo
{

class ThreadPool : boost::noncopyable
{
 public:
  typedef boost::function<void ()> Task;

  explicit ThreadPool(const string& name = string());
  ~ThreadPool();

  void start(int numThreads);
  void stop();

  void run(const Task& f);

 private:
  void runInThread();
  Task take();

  MutexLock mutex_;
  Condition cond_;
  string name_;
  
  //ptr_vector,内部存放Thread指针, 当其销毁时，会自动销毁每个指针指向的对象
  boost::ptr_vector<muduo::Thread> threads_;
  //存放任务
  std::deque<Task> queue_;

  //是否调用了start函数开始创建线程
  bool running_;
};

}

#endif
