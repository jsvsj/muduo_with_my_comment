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

//__thread修饰gcc内置线程局部存储设施
//只能修饰POD(plain old data)类型，与c兼容的原始数据

//线程局部存储的变量，每个线程都有一个，在线程范围之中是全局的

//缓存线程在系统中的真实tid,避免每次系统调用，浪费时间
  __thread int t_cachedTid = 0;

//tid的字符串表现形式
  __thread char t_tidString[32];

//线程名称
  __thread const char* t_threadName = "unknown";

//检测pid_t与int是否是相同类型,相同则返回true
  const bool sameType = boost::is_same<int, pid_t>::value;

//断言类型相同
  BOOST_STATIC_ASSERT(sameType);
}


namespace detail
{

//获取在系统中的唯一标识tid,线程在系统中的唯一标识
pid_t gettid()
{
  return static_cast<pid_t>(::syscall(SYS_gettid));
}

//在调用fork()函数之后子进程调用的函数
//如果再子线程中调用fork()函数，则产生的子进程中只有这个子线程,因此要把
void afterFork()
{
  muduo::CurrentThread::t_cachedTid = 0;
  muduo::CurrentThread::t_threadName = "main";
  CurrentThread::tid();

  //函数
  //pthread_atfork(void (*prepare)(void),void (*parent)(void),void(*child)(void));
  //调用fork()函数时，内部创建子进程前会调用prepare()函数,创建子进程后，在父进程中
  //会调用parent()函数,子进程中会调用child()函数
  
  // no need to call pthread_atfork(NULL, NULL, &afterFork);
}

//实际上，要么用多进程，要么用多线程

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


//在命名空间 CurrentThread中声明的函数
void CurrentThread::cacheTid()
{
  if (t_cachedTid == 0)
  {
  //得到线程的tid,并缓存
    t_cachedTid = detail::gettid();
  //缓存到字符串中
    int n = snprintf(t_tidString, sizeof t_tidString, "%5d ", t_cachedTid);

  //(void)n,防止n未被使用，编译时报错
	assert(n == 6); (void) n;
  }
}

//是否是主线程
bool CurrentThread::isMainThread()
{
  return tid() == ::getpid();
}

//创建的该线程对象的个数
AtomicInt32 Thread::numCreated_;

//线程构造函数
Thread::Thread(const ThreadFunc& func, const string& n)
  : started_(false),
    pthreadId_(0),
    tid_(0),
    func_(func),
    name_(n)
{
//线程个数+1
  numCreated_.increment();
}

Thread::~Thread()
{
  // no join
}

//启动线程，实质是创建创建线程，因为pthread_create()在创建线程的同时又会启动线程，
//调用startThread()函数，startThread函数用static修饰，这样才不会包含this指针
void Thread::start()
{
  assert(!started_);
  started_ = true;
  //在startThread中传入了this指针，便于使用类的属性,调用runInThread()函数
  errno = pthread_create(&pthreadId_, NULL, &startThread, this);
  if (errno != 0)
  {
    LOG_SYSFATAL << "Failed in pthread_create";
  }
}

//等待线程结束
int Thread::join()
{
  assert(started_);
  return pthread_join(pthreadId_, NULL);
}

//线程中执行的函数,其中有调用了runInThread()
void* Thread::startThread(void* obj)
{
  Thread* thread = static_cast<Thread*>(obj);
  
  thread->runInThread();

  return NULL;
}

//线程函数调用的函数
void Thread::runInThread()
{
//缓存tid
  tid_ = CurrentThread::tid();

  muduo::CurrentThread::t_threadName = name_.c_str();
  try
  {
  //调用真正的执行业务的回调函数
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

