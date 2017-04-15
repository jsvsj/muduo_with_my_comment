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

//���ɿ���
class MutexLock : boost::noncopyable
{
 public:
 	//���캯��
  MutexLock()
    : holder_(0)
  {
	//��ʼ��������
	int ret = pthread_mutex_init(&mutex_, NULL);
	assert(ret == 0); (void) ret;
  }

//��������
  ~MutexLock()
  {
  //���Ը���û�б��κ��߳�����
    assert(holder_ == 0);
	//���ٻ�����
    int ret = pthread_mutex_destroy(&mutex_);
    assert(ret == 0); (void) ret;
  }

//�Ƿ�ǰ�߳�ӵ�и���
  bool isLockedByThisThread()
  {
    return holder_ == CurrentThread::tid();
  }

//���Ե�ǰ�߳�ӵ�и���
  void assertLocked()
  {
    assert(isLockedByThisThread());
  }

  // internal usage

//����
  void lock()
  {
    pthread_mutex_lock(&mutex_);
//��ȡӵ�и������̵߳�tid
	holder_ = CurrentThread::tid();
  }

//����
  void unlock()
  {
    holder_ = 0;
    pthread_mutex_unlock(&mutex_);
  }

//��ȡ������
  pthread_mutex_t* getPthreadMutex() /* non-const */
  {
    return &mutex_;
  }

 private:

//������
  pthread_mutex_t mutex_;

//ӵ�и������߳�,Ϊ0ʱ��ʾ����û�б��κ��߳�����
  pid_t holder_;
};

//����RAII������װ��Mutex��
class MutexLockGuard : boost::noncopyable
{
 public:
 	//����ʱ������
  explicit MutexLockGuard(MutexLock& mutex)
    : mutex_(mutex)
  {
    mutex_.lock();
  }
//����ʱ����
  ~MutexLockGuard()
  {
    mutex_.unlock();
  }

 private:

//MutexLockGuard�ಢ������mutex�Ķ������������,������ϵ
  MutexLock& mutex_;
};

}

// Prevent misuse like:
// MutexLockGuard(mutex_);
// A tempory object doesn't hold the lock for long!
//��ֹ����һ��������Mutex������MutexLockGuard��
#define MutexLockGuard(x) error "Missing guard object name"

#endif  // MUDUO_BASE_MUTEX_H
