#include <muduo/base/AsyncLogging.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/Timestamp.h>

#include <stdio.h>

using namespace muduo;


//���캯��
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
//����������������
  currentBuffer_->bzero();
  nextBuffer_->bzero();
  //�������б��ʼ����С
  buffers_.reserve(16);
}

//ǰ���������̵߳��ô˺�������־����д�뻺����
void AsyncLogging::append(const char* logline, int len)
{
//��������
  muduo::MutexLockGuard lock(mutex_);

  //�����ǰ�������ռ乻��,ֱ�ӽ�����д�뻺����
  if (currentBuffer_->avail() > len)
  {
    currentBuffer_->append(logline, len);
  }
  else
  {
  //����ǰ��������ӵ���д���ļ��Ļ������б�
  //currentBuffer_.release()��currentBufferû��ָ����
    buffers_.push_back(currentBuffer_.release());

//����ǰ����������ΪԤ��������
    if (nextBuffer_)
    {
    //����������ָ�벻��=,����
    //�ƶ�����,j��currentBufferָ��nextBufferָ��Ļ������� nextBufferû��ָ����
      currentBuffer_ = boost::ptr_container::move(nextBuffer_);
      
    }
    else
    {
    //����������ٷ�����ǰ��д��̫�죬һ���Ӱ���������������
    //ֻ���ٴη���һ��������
      currentBuffer_.reset(new Buffer); // Rarely happens
    }
    currentBuffer_->append(logline, len);
	//֪ͨ��˿�ʼд����־
    cond_.notify();
  }
}

//�����־�߳�д�����ݵ��ļ�
void AsyncLogging::threadFunc()
{
  assert(running_ == true);
  
  latch_.countDown();

//�����ļ�
  LogFile output(basename_, rollSize_, false);

//׼��������еĻ�����
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

	  //�ǳ��������,һ����while��if���ܵ�����ٻ��ѣ��������ˣ���������û������
	  //������Ϊ���ڳ�ʱʱ�ܹ�����
      if (buffers_.empty())  // unusual usage!
      {
      //�ȴ�ǰ��д����һ�����������������߳�ʱ
        cond_.waitForSeconds(flushInterval_);
      }

	  //����ǰ���������뵽buffers_
      buffers_.push_back(currentBuffer_.release());

	  //�����е�newBuffer1����Ϊ��ǰ������
      currentBuffer_ = boost::ptr_container::move(newBuffer1);

//buffers��buffersToWriter��������������Ĵ���������ٽ���֮�ⰲȫ��д
//ֻ�ǽ���ָ�룬��û�н������ݣ�����Ч�ʽϸ�
	  buffersToWrite.swap(buffers_);

	  if (!nextBuffer_)
      {
      //ȷ��ǰ��ʼ����Ԥ������������,���ü���ǰ���ڴ������ʣ������ٽ�������
        nextBuffer_ = boost::ptr_container::move(newBuffer2);
      }
    }

    assert(!buffersToWrite.empty());

//��Ϣ�ѻ�
//ǰ��������ѭ����ƴ��������־��Ϣ��������˴������������ǵ��͵������ٶ�
//���������ٶ����⣬������������ڴ��жѻ�������ʱ������������(�����ڴ治��)
//��������(�����ڴ�ʧ��)

//��д�Ļ���������>25
    if (buffersToWrite.size() > 25)
    {
      char buf[256];
	  
      snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toFormattedString().c_str(),
               buffersToWrite.size()-2);
      fputs(buf, stderr);

	  output.append(buf, static_cast<int>(strlen(buf)));

	  //����������־�����ڳ��ڴ�ռ䣬����������������������
      buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end());
    }

//������������д����־�ļ�
    for (size_t i = 0; i < buffersToWrite.size(); ++i)
    {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      output.append(buffersToWrite[i].data(), buffersToWrite[i].length());
    }

//������������������,����newbuffer1��newbuffer2
    if (buffersToWrite.size() > 2)
    {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2);
    }


//��newbuffer1ָ��û�����
    if (!newBuffer1)
    {
      assert(!buffersToWrite.empty());
      newBuffer1 = buffersToWrite.pop_back();
      newBuffer1->reset();
    }

//��newbuffer2ָ��û�����
    if (!newBuffer2)
    {
      assert(!buffersToWrite.empty());
      newBuffer2 = buffersToWrite.pop_back();
      newBuffer2->reset();
    }

//������ж���ģ������
    buffersToWrite.clear();

	//ˢ��,д���ļ�
    output.flush();
  }
  output.flush();
}

