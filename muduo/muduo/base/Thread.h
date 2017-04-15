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

//�Ƿ������߳�
  bool started() const { return started_; }
  // pthread_t pthreadId() const { return pthreadId_; }
//��ȡ�߳�tid
  pid_t tid() const { return tid_; }

//��ȡ�߳�����
  const string& name() const { return name_; }

//��ȡ�������̵߳ĸ���
  static int numCreated() { return numCreated_.get(); }

 private:
 	//pthread_create()�����еĻص�����
  static void* startThread(void* thread);
	
  void runInThread();

//�߳��Ƿ�����
  bool       started_;

  //�߳�id
  pthread_t  pthreadId_;

//�߳���ϵͳ�е���ʵid
  pid_t      tid_;

//�߳�ִ�еĺ���
  ThreadFunc func_;

//�߳���
  string     name_;

//�����ĸ��̵߳Ķ���ĸ���
  static AtomicInt32 numCreated_;
};

}
#endif
