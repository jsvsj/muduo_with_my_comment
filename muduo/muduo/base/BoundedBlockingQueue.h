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

//有界队列的封装，模拟生产者，消费者
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

//添加数据
  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    while (queue_.full())
    {
      notFull_.wait();
    }
    assert(!queue_.full());
    queue_.push_back(x);
	//不空
    notEmpty_.notify(); // TODO: move outside of lock
  }

//获取数据
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
	//不满
    notFull_.notify(); // TODO: move outside of lock
    return front;
  }

//是否为空
  bool empty() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.empty();
  }
//环形缓冲区是否已经满
  bool full() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.full();
  }

//返回缓冲区中的元素的个数
  size_t size() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

//返回环形缓冲区的容量
  size_t capacity() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.capacity();
  }

 private:
 	//锁对象
  mutable MutexLock          mutex_;
  //不满
  Condition                  notEmpty_;
  //不空
  Condition                  notFull_;
  //采用boost库中的circular_buffer环形缓冲区
  boost::circular_buffer<T>  queue_;
};

}

#endif  // MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
