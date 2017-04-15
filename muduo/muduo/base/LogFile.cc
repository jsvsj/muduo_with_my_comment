#include <muduo/base/LogFile.h>
#include <muduo/base/Logging.h> // strerror_tl
#include <muduo/base/ProcessInfo.h>

#include <assert.h>
#include <stdio.h>
#include <time.h>

using namespace muduo;

// not thread safe
//日志文件LogFile中的File类
class LogFile::File : boost::noncopyable
{
 public:
 	//File类的构造函数,创建了一个新的文件
  explicit File(const string& filename)
  	//e代表调用exec后文件描述符关闭
    : fp_(::fopen(filename.data(), "ae")),
      writtenBytes_(0)
  {
    assert(fp_);
	//设置文件指针的缓冲区大小，否则为默认大小,当写入的字节数达到缓冲区大小时会自动刷新缓冲区
    ::setbuffer(fp_, buffer_, sizeof buffer_);
    // posix_fadvise POSIX_FADV_DONTNEED ?
  }

//析构函数，关闭文件指针
  ~File()
  {
    ::fclose(fp_);
  }

//将loglne追加写入日志文件
  void append(const char* logline, const size_t len)
  {
    size_t n = write(logline, len);
    size_t remain = len - n;
	//如果没有写完
    while (remain > 0)
    {
      size_t x = write(logline + n, remain);
      if (x == 0)
      {
        int err = ferror(fp_);
        if (err)
        {
          fprintf(stderr, "LogFile::File::append() failed %s\n", strerror_tl(err));
        }
        break;
      }
      n += x;
      remain = len - n; // remain -= x
    }

    writtenBytes_ += len;
  }

//刷新文件指针的缓冲区
  void flush()
  {
    ::fflush(fp_);
  }

//返回写入的字节数
  size_t writtenBytes() const { return writtenBytes_; }

 private:

//append()调用的函数
  size_t write(const char* logline, size_t len)
  {
#undef fwrite_unlocked
//不加锁写入，效率更高
    return ::fwrite_unlocked(logline, 1, len, fp_);
  }

//文件指针
  FILE* fp_;

  //文件指针的缓冲区
  char buffer_[64*1024];

//已经写入的字节数
  size_t writtenBytes_;
};


//构造函数
LogFile::LogFile(const string& basename,
                 size_t rollSize,
                 bool threadSafe,
                 int flushInterval)
  : basename_(basename),
    rollSize_(rollSize),
    flushInterval_(flushInterval),
    count_(0),
    mutex_(threadSafe ? new MutexLock : NULL),
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0)
{
//断言basename中没有/
  assert(basename.find('/') == string::npos);

//会初始化file指针，创建日志文件
  rollFile();
}

LogFile::~LogFile()
{
}

//往日志文件中添加数据
void LogFile::append(const char* logline, int len)
{
//如果是线程安全的
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    append_unlocked(logline, len);
  }
  else
  {
    append_unlocked(logline, len);
  }
}

//刷新文件指针的缓冲区
void LogFile::flush()
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    file_->flush();
  }
  else
  {
    file_->flush();
  }
}


//append()调用
void LogFile::append_unlocked(const char* logline, int len)
{
//向日志文件中添加数据
  file_->append(logline, len);

//如果已经写入的字节数大于要开始滚动的文件的大小
  if (file_->writtenBytes() > rollSize_)
  {
  //滚动日志
    rollFile();
  }
  else
  {
  
    if (count_ > kCheckTimeRoll_)
    {
      count_ = 0;
      time_t now = ::time(NULL);
	  
      time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
	  //如果时间过了一天,则滚动日志文件
      if (thisPeriod_ != startOfPeriod_)
      {
        rollFile();
      }
	  //如果刷新是时间间隔超过了flushInterval_,则刷新缓冲区
      else if (now - lastFlush_ > flushInterval_)
      {
        lastFlush_ = now;
        file_->flush();
      }
    }
    else
    {
    //计数值+1
      ++count_;
    }
  }
}

//滚动日志文件
void LogFile::rollFile()
{
  time_t now = 0;
  //获取日志文件名,now是输出参数，是创建文件名的时间
  string filename = getLogFileName(basename_, &now);

//start是一天的秒数的整数倍,也就是将日志文件创建时间调整到当天零点，为了方便
  time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

//如果now> 上次滚动日志的时间
  if (now > lastRoll_)
  {
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = start;
	//重置file_指针,创建了一个新的文件
	file_.reset(new File(filename));
  }
}

//获取日志文件名称,now是输出参数
string LogFile::getLogFileName(const string& basename, time_t* now)
{
  string filename;
  //保留多大的空间
  filename.reserve(basename.size() + 64);

  filename = basename;

  char timebuf[32];
  char pidbuf[32];
  struct tm tm;
  //获取当前时间，即距离1970.....的秒数
  *now = time(NULL);

  //根据秒数得到年月日时分秒,保存到结构体中
  gmtime_r(now, &tm); // FIXME: localtime_r ?

//格式化时间，输出到timebuf中
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);

  filename += timebuf;
  
  filename += ProcessInfo::hostname();

//进程号
  snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());

  filename += pidbuf;

  filename += ".log";

  return filename;
}

