// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_COUNTDOWNLATCH_H
#define MUDUO_BASE_COUNTDOWNLATCH_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>

namespace muduo
{

//�ȿ��������������̵߳ȴ����̷߳�������
//Ҳ�����������̵߳ȴ����̳߳�ʼ����ϲſ�ʼ����
class CountDownLatch : boost::noncopyable
{
 public:

  explicit CountDownLatch(int count);

  void wait();

  void countDown();

  int getCount() const;

 private:
 	//������,mutable,����ʹconst CountDownLatch *this���͵�this�ı�mutex_��ֵ
  mutable MutexLock mutex_;
	//������������
  Condition condition_;
	//��������
  int count_;
};

}
#endif  // MUDUO_BASE_COUNTDOWNLATCH_H
