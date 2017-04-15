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
 	//构造函数
  LogFile(const string& basename,
          size_t rollSize,
          //默认为线程安全的
          bool threadSafe = true,
          int flushInterval = 3);
  ~LogFile();

//将一行添加到日志文件中
  void append(const char* logline, int len);

  //刷新
  void flush();

 private:
 	//加锁的方式将一行添加到日志文件中
  void append_unlocked(const char* logline, int len);

//获取日志文件名称
  static string getLogFileName(const string& basename, time_t* now);

  //滚动日志文件
  void rollFile();

//日志文件的basename,如/aaa/sss/ccc.c中basename就是ccc.c
  const string basename_;

//日志文件大小达到rollSize就换一个文件
  const size_t rollSize_;

//日志写入的间隔时间
  const int flushInterval_;

//计数器
  int count_;

  boost::scoped_ptr<MutexLock> mutex_;

  //开始记录日志的时间(调整到零点)
  time_t startOfPeriod_;

//上一次滚动日志文件的时间
  time_t lastRoll_;

//上一次日志写入文件的时间
  time_t lastFlush_;

  //前向声明
  class File;
  
  //指向File的智能指针
  boost::scoped_ptr<File> file_;

  const static int kCheckTimeRoll_ = 1024;

  //一天的秒数
  const static int kRollPerSeconds_ = 60*60*24;
};

}
#endif  // MUDUO_BASE_LOGFILE_H
