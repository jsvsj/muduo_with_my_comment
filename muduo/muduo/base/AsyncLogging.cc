#include <muduo/base/AsyncLogging.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/Timestamp.h>

#include <stdio.h>

using namespace muduo;


//构造函数
AsyncLogging::AsyncLogging(const string& basename,
                           size_t rollSize,
                           int flushInterval)
  : flushInterval_(flushInterval),
    running_(false),
    basename_(basename),
    rollSize_(rollSize),

    thread_(boost::bind(&AsyncLogging::threadFunc, this), "Logging"),
    latch_(1),
    mutex_(),
    cond_(mutex_),

    currentBuffer_(new Buffer),
    nextBuffer_(new Buffer),
    buffers_()
{
//将缓冲区数据清零
  currentBuffer_->bzero();
  nextBuffer_->bzero();
  //缓冲区列表初始化大小
  buffers_.reserve(16);
}

//前端生产者线程调用此函数将日志数据写入缓冲区
void AsyncLogging::append(const char* logline, int len)
{
//加锁保护
  muduo::MutexLockGuard lock(mutex_);

  //如果当前缓冲区空间够用,直接将数据写入缓冲区
  if (currentBuffer_->avail() > len)
  {
    currentBuffer_->append(logline, len);
  }
  else
  {
  //将当前缓冲区添加到待写入文件的缓冲区列表
  //currentBuffer_.release()后currentBuffer没有指向了
    buffers_.push_back(currentBuffer_.release());

//将当前缓冲区设置为预备缓冲区
    if (nextBuffer_)
    {
    //该类型智能指针不能=,拷贝
    //移动语意,j将currentBuffer指向nextBuffer指向的缓冲区， nextBuffer没有指向了
      currentBuffer_ = boost::ptr_container::move(nextBuffer_);
      
    }
    else
    {
    //这种情况极少发生，前段写入太快，一下子把两个缓冲区用完
    //只能再次分配一个缓冲区
      currentBuffer_.reset(new Buffer); // Rarely happens
    }
    currentBuffer_->append(logline, len);
	//通知后端开始写入日志
    cond_.notify();
  }
}

//后端日志线程写入数据到文件
void AsyncLogging::threadFunc()
{
  assert(running_ == true);
  
  latch_.countDown();

//创建文件
  LogFile output(basename_, rollSize_, false);

//准备两块空闲的缓冲区
  BufferPtr newBuffer1(new Buffer);
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();
  
  BufferVector buffersToWrite;
  buffersToWrite.reserve(16);

  while (running_)
  {
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      muduo::MutexLockGuard lock(mutex_);

	  //非常规的做法,一般用while，if可能导致虚假唤醒，即唤醒了，但条件并没有满足
	  //这里是为了在超时时能够唤醒
      if (buffers_.empty())  // unusual usage!
      {
      //等待前端写满了一个或多个缓冲区，或者超时
        cond_.waitForSeconds(flushInterval_);
      }

	  //将当前缓冲区移入到buffers_
      buffers_.push_back(currentBuffer_.release());

	  //将空闲的newBuffer1设置为当前缓冲区
      currentBuffer_ = boost::ptr_container::move(newBuffer1);

//buffers与buffersToWriter交换，这样后面的代码可以在临界区之外安全的写
//只是交换指针，并没有交换数据，所以效率较高
	  buffersToWrite.swap(buffers_);

	  if (!nextBuffer_)
      {
      //确保前段始终有预备缓冲区可用,可用减少前段内存分配概率，缩短临界区长度
        nextBuffer_ = boost::ptr_container::move(newBuffer2);
      }
    }

    assert(!buffersToWrite.empty());

//消息堆积
//前端陷入死循环，拼命发生日志消息，超过后端处理能力，这是典型的生产速度
//大于消费速度问题，会造成数据在内存中堆积，严重时引发性能问题(可用内存不足)
//或程序崩溃(分配内存失败)

//待写的缓冲区数量>25
    if (buffersToWrite.size() > 25)
    {
      char buf[256];
	  
      snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toFormattedString().c_str(),
               buffersToWrite.size()-2);
      fputs(buf, stderr);

	  output.append(buf, static_cast<int>(strlen(buf)));

	  //丢掉多余日志，以腾出内存空间，仅仅保留两个缓冲区数据
      buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end());
    }

//将缓冲区数据写入日志文件
    for (size_t i = 0; i < buffersToWrite.size(); ++i)
    {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      output.append(buffersToWrite[i].data(), buffersToWrite[i].length());
    }

//仅仅保留两个缓冲区,用于newbuffer1和newbuffer2
    if (buffersToWrite.size() > 2)
    {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2);
    }


//将newbuffer1指向该缓冲区
    if (!newBuffer1)
    {
      assert(!buffersToWrite.empty());
      newBuffer1 = buffersToWrite.pop_back();
      newBuffer1->reset();
    }

//将newbuffer2指向该缓冲区
    if (!newBuffer2)
    {
      assert(!buffersToWrite.empty());
      newBuffer2 = buffersToWrite.pop_back();
      newBuffer2->reset();
    }

//如果还有多余的，则清除
    buffersToWrite.clear();

	//刷新,写入文件
    output.flush();
  }
  output.flush();
}

