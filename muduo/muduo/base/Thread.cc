// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Thread.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/base/Exception.h>
#include <muduo/base/Logging.h>

#include <boost/static_assert.hpp>
#include <boost/type_traits/is_same.hpp>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/unistd.h>

namespace muduo
{
namespace CurrentThread
{

//__thread����gcc�����ֲ߳̾��洢��ʩ
//ֻ������POD(plain old data)���ͣ���c���ݵ�ԭʼ����

//�ֲ߳̾��洢�ı�����ÿ���̶߳���һ�������̷߳�Χ֮����ȫ�ֵ�

//�����߳���ϵͳ�е���ʵtid,����ÿ��ϵͳ���ã��˷�ʱ��
  __thread int t_cachedTid = 0;

//tid���ַ���������ʽ
  __thread char t_tidString[32];

//�߳�����
  __thread const char* t_threadName = "unknown";

//���pid_t��int�Ƿ�����ͬ����,��ͬ�򷵻�true
  const bool sameType = boost::is_same<int, pid_t>::value;

//����������ͬ
  BOOST_STATIC_ASSERT(sameType);
}


namespace detail
{

//��ȡ��ϵͳ�е�Ψһ��ʶtid,�߳���ϵͳ�е�Ψһ��ʶ
pid_t gettid()
{
  return static_cast<pid_t>(::syscall(SYS_gettid));
}

//�ڵ���fork()����֮���ӽ��̵��õĺ���
//��������߳��е���fork()��������������ӽ�����ֻ��������߳�,���Ҫ��
void afterFork()
{
  muduo::CurrentThread::t_cachedTid = 0;
  muduo::CurrentThread::t_threadName = "main";
  CurrentThread::tid();

  //����
  //pthread_atfork(void (*prepare)(void),void (*parent)(void),void(*child)(void));
  //����fork()����ʱ���ڲ������ӽ���ǰ�����prepare()����,�����ӽ��̺��ڸ�������
  //�����parent()����,�ӽ����л����child()����
  
  // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

//ʵ���ϣ�Ҫô�ö���̣�Ҫô�ö��߳�

class ThreadNameInitializer
{
 public:
  ThreadNameInitializer()
  {
    muduo::CurrentThread::t_threadName = "main";
    CurrentThread::tid();
	
    pthread_atfork(NULL, NULL, &afterFork);
  }
};

ThreadNameInitializer init;
}
}

using namespace muduo;


//�������ռ� CurrentThread�������ĺ���
void CurrentThread::cacheTid()
{
  if (t_cachedTid == 0)
  {
  //�õ��̵߳�tid,������
    t_cachedTid = detail::gettid();
  //���浽�ַ�����
    int n = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);

  //(void)n,��ֹnδ��ʹ�ã�����ʱ����
	assert(n == 6); (void) n;
  }
}

//�Ƿ������߳�
bool CurrentThread::isMainThread()
{
  return tid() == ::getpid();
}

//�����ĸ��̶߳���ĸ���
AtomicInt32 Thread::numCreated_;

//�̹߳��캯��
Thread::Thread(const ThreadFunc& func, const string& n)
  : started_(false),
    pthreadId_(0),
    tid_(0),
    func_(func),
    name_(n)
{
//�̸߳���+1
  numCreated_.increment();
}

Thread::~Thread()
{
  // no join
}

//�����̣߳�ʵ���Ǵ��������̣߳���Ϊpthread_create()�ڴ����̵߳�ͬʱ�ֻ������̣߳�
//����startThread()������startThread������static���Σ������Ų������thisָ��
void Thread::start()
{
  assert(!started_);
  started_ = true;
  //��startThread�д�����thisָ�룬����ʹ���������,����runInThread()����
  errno = pthread_create(&pthreadId_, NULL, &startThread, this);
  if (errno != 0)
  {
    LOG_SYSFATAL << "Failed in pthread_create";
  }
}

//�ȴ��߳̽���
int Thread::join()
{
  assert(started_);
  return pthread_join(pthreadId_, NULL);
}

//�߳���ִ�еĺ���,�����е�����runInThread()
void* Thread::startThread(void* obj)
{
  Thread* thread = static_cast<Thread*>(obj);
  
  thread->runInThread();

  return NULL;
}

//�̺߳������õĺ���
void Thread::runInThread()
{
//����tid
  tid_ = CurrentThread::tid();

  muduo::CurrentThread::t_threadName = name_.c_str();
  try
  {
  //����������ִ��ҵ��Ļص�����
    func_();
  
    muduo::CurrentThread::t_threadName = "finished";
  }
  catch (const Exception& ex)
  {
    muduo::CurrentThread::t_threadName = "crashed";
    fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    muduo::CurrentThread::t_threadName = "crashed";
    fprintf(stderr, "exception caught in Thread %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    muduo::CurrentThread::t_threadName = "crashed";
    fprintf(stderr, "unknown exception caught in Thread %s\n", name_.c_str());
    throw; // rethrow
  }
}

