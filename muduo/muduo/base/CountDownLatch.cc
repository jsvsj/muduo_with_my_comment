// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/CountDownLatch.h>

using namespace muduo;

//���캯��
CountDownLatch::CountDownLatch(int count)
  : mutex_(),
  //condition(&mutex),condition�ಢ������mutex�������������
    condition_(mutex_),
    count_(count)
{
}

//�ȴ���������Ϊ0
void CountDownLatch::wait()
{
  MutexLockGuard lock(mutex_);
  while (count_ > 0) {
    condition_.wait();
  }
}

//����������һ
void CountDownLatch::countDown()
{
  MutexLockGuard lock(mutex_);
  --count_;
  //�����������Ϊ0���������еȴ��ĸ��������߳�
  if (count_ == 0) {
    condition_.notifyAll();
  }
}

//��ȡ����������ֵ
int CountDownLatch::getCount() const
{
  MutexLockGuard lock(mutex_);
  return count_;
}

