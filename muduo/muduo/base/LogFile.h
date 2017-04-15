#ifndef MUDUO_BASE_LOGFILE_H
#define MUDUO_BASE_LOGFILE_H

#include <muduo/base/Mutex.h>
#include <muduo/base/Types.h>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{

class LogFile : boost::noncopyable
{
 public:
 	//���캯��
  LogFile(const string& basename,
          size_t rollSize,
          //Ĭ��Ϊ�̰߳�ȫ��
          bool threadSafe = true,
          int flushInterval = 3);
  ~LogFile();

//��һ����ӵ���־�ļ���
  void append(const char* logline, int len);

  //ˢ��
  void flush();

 private:
 	//�����ķ�ʽ��һ����ӵ���־�ļ���
  void append_unlocked(const char* logline, int len);

//��ȡ��־�ļ�����
  static string getLogFileName(const string& basename, time_t* now);

  //������־�ļ�
  void rollFile();

//��־�ļ���basename,��/aaa/sss/ccc.c��basename����ccc.c
  const string basename_;

//��־�ļ���С�ﵽrollSize�ͻ�һ���ļ�
  const size_t rollSize_;

//��־д��ļ��ʱ��
  const int flushInterval_;

//������
  int count_;

  boost::scoped_ptr<MutexLock> mutex_;

  //��ʼ��¼��־��ʱ��(���������)
  time_t startOfPeriod_;

//��һ�ι�����־�ļ���ʱ��
  time_t lastRoll_;

//��һ����־д���ļ���ʱ��
  time_t lastFlush_;

  //ǰ������
  class File;
  
  //ָ��File������ָ��
  boost::scoped_ptr<File> file_;

  const static int kCheckTimeRoll_ = 1024;

  //һ�������
  const static int kRollPerSeconds_ = 60*60*24;
};

}
#endif  // MUDUO_BASE_LOGFILE_H
