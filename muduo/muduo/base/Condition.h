// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CONDITION_H
#define MUDUO_BASE_CONDITION_H

#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo
{

//条件变量的类的封装
class Condition : boost::noncopyable
{
 public:
 	//构造函数，初始化条件变量
  explicit Condition(MutexLock& mutex)
    : mutex_(mutex)
  {
    pthread_cond_init(&pcond_, NULL);
  }

//析构函数,销毁条件变量
  ~Condition()
  {
    pthread_cond_destroy(&pcond_);
  }

//等待
  void wait()
  {
    pthread_cond_wait(&pcond_, mutex_.getPthreadMutex());
  }

  // returns true if time out, false otherwise.
  //等待一段时间 
  bool waitForSeconds(int seconds);

//唤醒
  void notify()
  {
    pthread_cond_signal(&pcond_);
  }

//唤醒所有等待的线程
  void notifyAll()
  {
    pthread_cond_broadcast(&pcond_);
  }

 private:
 	//锁对象，Condition类并不管理mutex_的生命周期
  MutexLock& mutex_;
	//条件变量
  pthread_cond_t pcond_;
};

}
#endif  // MUDUO_BASE_CONDITION_H
