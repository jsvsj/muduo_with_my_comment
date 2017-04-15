// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
#define MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/circular_buffer.hpp>
#include <boost/noncopyable.hpp>
#include <assert.h>

namespace muduo
{

//�н���еķ�װ��ģ�������ߣ�������
template<typename T>
class BoundedBlockingQueue : boost::noncopyable
{
 public:
  explicit BoundedBlockingQueue(int maxSize)
    : mutex_(),
      notEmpty_(mutex_),
      notFull_(mutex_),
      queue_(maxSize)
  {
  }

//�������
  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    while (queue_.full())
    {
      notFull_.wait();
    }
    assert(!queue_.full());
    queue_.push_back(x);
	//����
    notEmpty_.notify(); // TODO: move outside of lock
  }

//��ȡ����
  T take()
  {
    MutexLockGuard lock(mutex_);
    while (queue_.empty())
    {
      notEmpty_.wait();
    }
    assert(!queue_.empty());
    T front(queue_.front());
    queue_.pop_front();
	//����
    notFull_.notify(); // TODO: move outside of lock
    return front;
  }

//�Ƿ�Ϊ��
  bool empty() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.empty();
  }
//���λ������Ƿ��Ѿ���
  bool full() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.full();
  }

//���ػ������е�Ԫ�صĸ���
  size_t size() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

//���ػ��λ�����������
  size_t capacity() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.capacity();
  }

 private:
 	//������
  mutable MutexLock          mutex_;
  //����
  Condition                  notEmpty_;
  //����
  Condition                  notFull_;
  //����boost���е�circular_buffer���λ�����
  boost::circular_buffer<T>  queue_;
};

}

#endif  // MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
