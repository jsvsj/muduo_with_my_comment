#ifndef MUDUO_BASE_ASYNCLOGGING_H
#define MUDUO_BASE_ASYNCLOGGING_H

#include <muduo/base/BlockingQueue.h>
#include <muduo/base/BoundedBlockingQueue.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>

#include <muduo/base/LogStream.h>

#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace muduo
{


//异步日志类
class AsyncLogging : boost::noncopyable
{
 public:

  AsyncLogging(const string& basename,
               size_t rollSize,
               int flushInterval = 3);

  ~AsyncLogging()
  {
    if (running_)
    {
      stop();
    }
  }

  
//前段生产者线程使用，将数据写入到缓冲区
  void append(const char* logline, int len);

  void start()
  {
    running_ = true;
	//日志线程启动
    thread_.start();
	
    latch_.wait();
  }

  void stop()
  {
    running_ = false;
    cond_.notify();
    thread_.join();
  }

 private:

  // declare but not define, prevent compiler-synthesized functions
  AsyncLogging(const AsyncLogging&);  // ptr_container

  void operator=(const AsyncLogging&);  // ptr_container


//供后端消费者回调，将数据写入到日志
  void threadFunc();

//缓冲区类型
  typedef muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer> Buffer;

  //缓冲区列表,ptr_vector中存放Buffer的指针。并且能够管理指针指向对象的生存周期
  typedef boost::ptr_vector<Buffer> BufferVector;

//可以理解为buffer的智能指针，可以管理buffer的生存期,类似于c++11中的unick_ptr,不能=,拷贝
  typedef BufferVector::auto_type BufferPtr;


//超时时间，如果在flushInterval_秒内，缓冲区没有写满，仍将缓冲区数据写入文件
  const int flushInterval_;
  
  bool running_;
  
  string basename_;//日志文件basename

  size_t rollSize_;//日志文件滚动大小

  muduo::Thread thread_;

  muduo::CountDownLatch latch_;//用于等待线程启动
  
  muduo::MutexLock mutex_;
  muduo::Condition cond_;
  
  BufferPtr currentBuffer_;//当前缓冲区

  BufferPtr nextBuffer_;//预备缓冲区

  BufferVector buffers_;//待写入文件的已经填满的缓冲区
};

}
#endif  // MUDUO_BASE_ASYNCLOGGING_H
