#include <muduo/base/ThreadPool.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/base/CurrentThread.h>

#include <boost/bind.hpp>
#include <stdio.h>

void print()
{
  printf("tid=%d\n", muduo::CurrentThread::tid());
}

void printString(const std::string& str)
{
  printf("tid=%d, str=%s\n", muduo::CurrentThread::tid(), str.c_str());
}

int main()
{
//创建线程池
  muduo::ThreadPool pool("MainThreadPool");
//创建5个线程
  pool.start(5);
//添加任务
  pool.run(print);
  pool.run(print);
  
  for (int i = 0; i < 100; ++i)
  {
    char buf[32];
    snprintf(buf, sizeof buf, "task %d", i);
//添加任务
	pool.run(boost::bind(printString, std::string(buf)));
  }

//计数门闩
  muduo::CountDownLatch latch(1);
//添加任务
  pool.run(boost::bind(&muduo::CountDownLatch::countDown, &latch));

  latch.wait();
//结束线程池
  pool.stop();
}

