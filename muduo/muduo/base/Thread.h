// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREAD_H
#define MUDUO_BASE_THREAD_H

#include <muduo/base/Atomic.h>
#include <muduo/base/Types.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo
{
 
class Thread : boost::noncopyable
{
 public:
  typedef boost::function<void ()> ThreadFunc;

  explicit Thread(const ThreadFunc&, const string& name = string());
  ~Thread();

  void start();
  int join(); // return pthread_join()

//是否启动线程
  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
//获取线程tid
  pid_t tid() const { return tid_; }

//获取线程名称
  const string& name() const { return name_; }

//获取创建的线程的个数
  static int numCreated() { return numCreated_.get(); }

 private:
 	//pthread_create()函数中的回调函数
  static void* startThread(void* thread);
	
  void runInThread();

//线程是否启动
  bool       started_;

  //线程id
  pthread_t  pthreadId_;

//线程在系统中的真实id
  pid_t      tid_;

//线程执行的函数
  ThreadFunc func_;

//线程名
  string     name_;

//创建的该线程的对象的个数
  static AtomicInt32 numCreated_;
};

}
#endif
