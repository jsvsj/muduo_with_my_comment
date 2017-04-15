// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BLOCKINGQUEUE_H
#define MUDUO_BASE_BLOCKINGQUEUE_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>
#include <deque>
#include <assert.h>

namespace muduo
{

//无界队列的封装，模拟生产者消费者模型
//使用模板
template<typename T>

class BlockingQueue : boost::noncopyable
{
 public:
  BlockingQueue()
    : mutex_(),
      notEmpty_(mutex_),
      queue_()
  {
  }

//往队列中添加元素
  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(x);
    notEmpty_.notify(); // TODO: move outside of lock
  }

//从队列中获取元素
  T take()
  {
    MutexLockGuard lock(mutex_);
    // always use a while-loop, due to spurious wakeup
	//等待条件成立
	while (queue_.empty())
    {
      notEmpty_.wait();
    }
    assert(!queue_.empty());
	//获取元素
    T front(queue_.front());
    queue_.pop_front();
    return front;
  }

//获取队列大小
  size_t size() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

 private:
 	//锁对象
  mutable MutexLock mutex_;
	//条件变量
  Condition         notEmpty_;
	//队列
  std::deque<T>     queue_;
};

}

#endif  // MUDUO_BASE_BLOCKINGQUEUE_H
