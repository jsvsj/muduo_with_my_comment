// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/CountDownLatch.h>

using namespace muduo;

//构造函数
CountDownLatch::CountDownLatch(int count)
  : mutex_(),
  //condition(&mutex),condition类并不管理mutex对象的生命周期
    condition_(mutex_),
    count_(count)
{
}

//等待计数变量为0
void CountDownLatch::wait()
{
  MutexLockGuard lock(mutex_);
  while (count_ > 0) {
    condition_.wait();
  }
}

//计数变量减一
void CountDownLatch::countDown()
{
  MutexLockGuard lock(mutex_);
  --count_;
  //如果计数变量为0，则唤醒所有等待的该条件的线程
  if (count_ == 0) {
    condition_.notifyAll();
  }
}

//获取计数变量的值
int CountDownLatch::getCount() const
{
  MutexLockGuard lock(mutex_);
  return count_;
}

