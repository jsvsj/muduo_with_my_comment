#ifndef MUDUO_BASE_LOGGING_H
#define MUDUO_BASE_LOGGING_H

#include <muduo/base/LogStream.h>
#include <muduo/base/Timestamp.h>

namespace muduo
{

class Logger
{
 public:
 	//��־����
  enum LogLevel
  {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
    NUM_LOG_LEVELS,
  };

  // compile time calculation of basename of source file
  class SourceFile
  {
   public:

//������ģ��
//���캯��
	template<int N>
    inline SourceFile(const char (&arr)[N])
      : data_(arr),
        size_(N-1)
    {
    //��ȡdata
      const char* slash = strrchr(data_, '/'); // builtin function
      if (slash)
      {
        data_ = slash + 1;
        size_ -= static_cast<int>(data_ - arr);
      }
    }

//���캯��
//��ȡdata
    explicit SourceFile(const char* filename)
      : data_(filename)
    {
      const char* slash = strrchr(filename, '/');
      if (slash)
      {
        data_ = slash + 1;
      }
      size_ = static_cast<int>(strlen(data_));
    }

    const char* data_;
    int size_;
  };

  Logger(SourceFile file, int line);
  Logger(SourceFile file, int line, LogLevel level);
  Logger(SourceFile file, int line, LogLevel level, const char* func);
  Logger(SourceFile file, int line, bool toAbort);
  ~Logger();

//����Impl�е�stream���������
  LogStream& stream() { return impl_.stream_; }

//��־��ȡ����
  static LogLevel logLevel();

  //������־����
  static void setLogLevel(LogLevel level);

//����ָ��
  typedef void (*OutputFunc)(const char* msg, int len);
  typedef void (*FlushFunc)();

  //������־�������
  static void setOutput(OutputFunc);

  //����ˢ�º���
  static void setFlush(FlushFunc);

 private:

//Ƕ����
class Impl
{
 public:
  typedef Logger::LogLevel LogLevel;
//���캯��
  Impl(LogLevel level, int old_errno, const SourceFile& file, int line);

  void formatTime();
  void finish();

  Timestamp time_;
  
  LogStream stream_;

  LogLevel level_;
  int line_;
  SourceFile basename_;
};

  Impl impl_;

};

extern Logger::LogLevel g_logLevel;

inline Logger::LogLevel Logger::logLevel()
{
  return g_logLevel;
}

#define LOG_TRACE if (muduo::Logger::logLevel() <= muduo::Logger::TRACE) \
	//���if�е��жϳ�������ִ�������䣬�����������󣬵���stream()����,����LogStream����
	//LogStream����������<<������,������֮��������������
	//__FILE__���д��������ļ���   
	//__LINE__���д��������к�
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::TRACE, __func__).stream()
  
#define LOG_DEBUG if (muduo::Logger::logLevel() <= muduo::Logger::DEBUG) \
  muduo::Logger(__FILE__, __LINE__, muduo::Logger::DEBUG, __func__).stream()

#define LOG_INFO if (muduo::Logger::logLevel() <= muduo::Logger::INFO) \
  muduo::Logger(__FILE__, __LINE__).stream()

#define LOG_WARN muduo::Logger(__FILE__, __LINE__, muduo::Logger::WARN).stream()

#define LOG_ERROR muduo::Logger(__FILE__, __LINE__, muduo::Logger::ERROR).stream()

#define LOG_FATAL muduo::Logger(__FILE__, __LINE__, muduo::Logger::FATAL).stream()

#define LOG_SYSERR muduo::Logger(__FILE__, __LINE__, false).stream()

#define LOG_SYSFATAL muduo::Logger(__FILE__, __LINE__, true).stream()

const char* strerror_tl(int savedErrno);

// Taken from glog/logging.h
//
// Check that the input is non NULL.  This very useful in constructor
// initializer lists.

#define CHECK_NOTNULL(val) \
  ::muduo::CheckNotNull(__FILE__, __LINE__, "'" #val "' Must be non NULL", (val))

// A small helper for CHECK_NOTNULL().
template <typename T>
T* CheckNotNull(Logger::SourceFile file, int line, const char *names, T* ptr) {
  if (ptr == NULL) {
   Logger(file, line, Logger::FATAL).stream() << names;
  }
  return ptr;
}

}

#endif  // MUDUO_BASE_LOGGING_H
