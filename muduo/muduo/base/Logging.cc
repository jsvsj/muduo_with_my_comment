#include <muduo/base/Logging.h>

#include <muduo/base/CurrentThread.h>
#include <muduo/base/StringPiece.h>
#include <muduo/base/Timestamp.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sstream>

namespace muduo
{

/*
class LoggerImpl
{
 public:
  typedef Logger::LogLevel LogLevel;
  LoggerImpl(LogLevel level, int old_errno, const char* file, int line);
  void finish();

  Timestamp time_;
  LogStream stream_;
  LogLevel level_;
  int line_;
  const char* fullname_;
  const char* basename_;
};
*/

__thread char t_errnobuf[512];
__thread char t_time[32];
__thread time_t t_lastSecond;

const char* strerror_tl(int savedErrno)
{
  return strerror_r(savedErrno, t_errnobuf, sizeof t_errnobuf);
}

Logger::LogLevel initLogLevel()
{
  if (::getenv("MUDUO_LOG_TRACE"))
    return Logger::TRACE;
  else if (::getenv("MUDUO_LOG_DEBUG"))
    return Logger::DEBUG;
  else
    return Logger::INFO;
}

Logger::LogLevel g_logLevel = initLogLevel();

const char* LogLevelName[Logger::NUM_LOG_LEVELS] =
{
  "TRACE ",
  "DEBUG ",
  "INFO  ",
  "WARN  ",
  "ERROR ",
  "FATAL ",
};

// helper class for known string length at compile time
class T
{
 public:
  T(const char* str, unsigned len)
    :str_(str),
     len_(len)
  {
    assert(strlen(str) == len_);
  }

  const char* str_;
  const unsigned len_;
};


//重载T类的<<函数
inline LogStream& operator<<(LogStream& s, T v)
{
//将s追加
  s.append(v.str_, v.len_);

  return s;
}

//重载T类的<<函数
inline LogStream& operator<<(LogStream& s, const Logger::SourceFile& v)
{
  s.append(v.data_, v.size_);
  return s;
}

//默认的输出，输出到标准输出上
void defaultOutput(const char* msg, int len)
{
  size_t n = fwrite(msg, 1, len, stdout);
  //FIXME check n
  (void)n;
}

//默认的刷新，刷新stdout
void defaultFlush()
{
  fflush(stdout);
}

//默认的
//OutputFunc
//FlushFunc函数指针
Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

}

using namespace muduo;

//Impl类的构造函数
Logger::Impl::Impl(LogLevel level, int savedErrno, const SourceFile& file, int line)
  : time_(Timestamp::now()),
    stream_(),
    level_(level),
    line_(line),
    basename_(file)
{
//格式化时间
  formatTime();
//获得线程tid
  CurrentThread::tid();

  stream_ << T(CurrentThread::tidString(), 6);

//格式化日志级别，即将日志级别获取对应的字符串
  stream_ << T(LogLevelName[level], 6);

//如果errno!=0
  if (savedErrno != 0)
  {
    stream_ << strerror_tl(savedErrno) << " (errno=" << savedErrno << ") ";
  }
}

//格式化时间
void Logger::Impl::formatTime()
{
//总微秒数
  int64_t microSecondsSinceEpoch = time_.microSecondsSinceEpoch();
//秒数部分
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch / 1000000);
//微秒数部分
  int microseconds = static_cast<int>(microSecondsSinceEpoch % 1000000);

  if (seconds != t_lastSecond)
  {
    t_lastSecond = seconds;
    struct tm tm_time;
//调用此函数格式化时间
	::gmtime_r(&seconds, &tm_time); // FIXME TimeZone::fromUtcTime

    int len = snprintf(t_time, sizeof(t_time), "%4d%02d%02d %02d:%02d:%02d",
        tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
        tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
    assert(len == 17); (void)len;
  }
  Fmt us(".%06dZ ", microseconds);
  assert(us.length() == 9);
  stream_ << T(t_time, 17) << T(us.data(), 9);
}

//在Logger的析构函数中被调用
void Logger::Impl::finish()
{
  stream_ << " - " << basename_ << ':' << line_ << '\n';
}

Logger::Logger(SourceFile file, int line)
  : impl_(INFO, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, LogLevel level, const char* func)
  : impl_(level, 0, file, line)
{
  impl_.stream_ << func << ' ';
}

Logger::Logger(SourceFile file, int line, LogLevel level)
  : impl_(level, 0, file, line)
{
}

Logger::Logger(SourceFile file, int line, bool toAbort)
  : impl_(toAbort?FATAL:ERROR, errno, file, line)
{
}

//析构函数,输出日志
Logger::~Logger()
{
//调用impl_的finish()
  impl_.finish();
  
  const LogStream::Buffer& buf(stream().buffer());

//调用输出g_output为函数指针
  g_output(buf.data(), buf.length());

  if (impl_.level_ == FATAL)
  {
  //刷新输出的缓冲区
    g_flush();
  
    abort();
  }
}

//设置日志级别
void Logger::setLogLevel(Logger::LogLevel level)
{
  g_logLevel = level;
}

//设置输出函数
void Logger::setOutput(OutputFunc out)
{
  g_output = out;
}

//设置刷新函数
void Logger::setFlush(FlushFunc flush)
{
  g_flush = flush;
}
