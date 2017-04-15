// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/ThreadPool.h>

#include <muduo/base/Exception.h>

#include <boost/bind.hpp>
#include <assert.h>
#include <stdio.h>

using namespace muduo;

//�̳߳ع��캯��
ThreadPool::ThreadPool(const string& name)
  : mutex_(),
    cond_(mutex_),
    name_(name),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
//�����û����
  if (running_)
  {
    stop();
  }
}

//�����̳߳أ�ʵ���Ǹ����̸߳����������ɸ��̶߳���Ȼ������ÿһ���߳�
void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;
  threads_.reserve(numThreads);
  //�����߳�
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i);

	//�����̶߳���,ʵ���ǳ�ʼ���̶߳���Ҫִ�еĻص���������runInThread�ȣ�������ӵ�vector��
    threads_.push_back(new muduo::Thread(
          boost::bind(&ThreadPool::runInThread, this), name_+id));
	//����ÿһ���߳�
	threads_[i].start();
  }
}

//����ʱ����
void ThreadPool::stop()
{
  {
  MutexLockGuard lock(mutex_);
  
  running_ = false;
  //�������еĵȴ��߳�
  cond_.notifyAll();
  }
//��ÿ���߳�ִ��join����,�ȴ�����
  for_each(threads_.begin(),
           threads_.end(),
           boost::bind(&muduo::Thread::join, _1));
}

//�������
//���̳߳��е���������������ص�����
void ThreadPool::run(const Task& task)
{
//����̳߳���û���̣߳���������߳�ִ������
  if (threads_.empty())
  {
    task();
  }
  //��������ӵ�queue��
  else
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(task);
	//֪ͨ����Ϊ��
    cond_.notify();
  }
}



//����������л�ȡһ�������൱��������
ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  //�ȴ���������߽�����־
  while (queue_.empty() && running_)
  {
    cond_.wait();
  }
  Task task;
  if(!queue_.empty())
  {
    task = queue_.front();
    queue_.pop_front();
  }
  return task;
}


//ÿһ���̶߳�����ִ�еĺ���
void ThreadPool::runInThread()
{
  try
  {
  //����stop()����֮��Ὣrunning_��Ϊfalse
    while (running_)
    {
    //����������л�ȡһ������Ȼ��ִ��
      Task task(take());
      if (task)
      {
      //ִ������ص�����
        task();
      }
    }
  }
  catch (const Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}

