// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_MUTEX_H
#define MUDUO_BASE_MUTEX_H

#include <muduo/base/CurrentThread.h>
#include <boost/noncopyable.hpp>
#include <assert.h>
#include <pthread.h>

namespace muduo
{

//不可拷贝
class MutexLock : boost::noncopyable
{
 public:
 	//构造函数
  MutexLock()
    : holder_(0)
  {
	//初始化互斥量
	int ret = pthread_mutex_init(&mutex_, NULL);
	assert(ret == 0); (void) ret;
  }

//析构函数
  ~MutexLock()
  {
  //断言该锁没有被任何线程利用
    assert(holder_ == 0);
	//销毁互斥量
    int ret = pthread_mutex_destroy(&mutex_);
    assert(ret == 0); (void) ret;
  }

//是否当前线程拥有该锁
  bool isLockedByThisThread()
  {
    return holder_ == CurrentThread::tid();
  }

//断言当前线程拥有该锁
  void assertLocked()
  {
    assert(isLockedByThisThread());
  }

  // internal usage

//加锁
  void lock()
  {
    pthread_mutex_lock(&mutex_);
//获取拥有该锁的线程的tid
	holder_ = CurrentThread::tid();
  }

//解锁
  void unlock()
  {
    holder_ = 0;
    pthread_mutex_unlock(&mutex_);
  }

//获取互斥量
  pthread_mutex_t* getPthreadMutex() /* non-const */
  {
    return &mutex_;
  }

 private:

//互斥量
  pthread_mutex_t mutex_;

//拥有该锁的线程,为0时表示该锁没有被任何线程利用
  pid_t holder_;
};

//采用RAII技法封装的Mutex类
class MutexLockGuard : boost::noncopyable
{
 public:
 	//构造时就上锁
  explicit MutexLockGuard(MutexLock& mutex)
    : mutex_(mutex)
  {
    mutex_.lock();
  }
//析构时解锁
  ~MutexLockGuard()
  {
    mutex_.unlock();
  }

 private:

//MutexLockGuard类并不负责mutex的对象的生命周期,关联关系
  MutexLock& mutex_;
};

}

// Prevent misuse like:
// MutexLockGuard(mutex_);
// A tempory object doesn't hold the lock for long!
//防止创建一个匿名的Mutex对象传入MutexLockGuard中
#define MutexLockGuard(x) error "Missing guard object name"

#endif  // MUDUO_BASE_MUTEX_H
