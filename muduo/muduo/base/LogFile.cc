#include <muduo/base/LogFile.h>
#include <muduo/base/Logging.h> // strerror_tl
#include <muduo/base/ProcessInfo.h>

#include <assert.h>
#include <stdio.h>
#include <time.h>

using namespace muduo;

// not thread safe
//��־�ļ�LogFile�е�File��
class LogFile::File : boost::noncopyable
{
 public:
 	//File��Ĺ��캯��,������һ���µ��ļ�
  explicit File(const string& filename)
  	//e�������exec���ļ��������ر�
    : fp_(::fopen(filename.data(), "ae")),
      writtenBytes_(0)
  {
    assert(fp_);
	//�����ļ�ָ��Ļ�������С������ΪĬ�ϴ�С,��д����ֽ����ﵽ��������Сʱ���Զ�ˢ�»�����
    ::setbuffer(fp_, buffer_, sizeof buffer_);
    // posix_fadvise POSIX_FADV_DONTNEED ?
  }

//�����������ر��ļ�ָ��
  ~File()
  {
    ::fclose(fp_);
  }

//��loglne׷��д����־�ļ�
  void append(const char* logline, const size_t len)
  {
    size_t n = write(logline, len);
    size_t remain = len - n;
	//���û��д��
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

//ˢ���ļ�ָ��Ļ�����
  void flush()
  {
    ::fflush(fp_);
  }

//����д����ֽ���
  size_t writtenBytes() const { return writtenBytes_; }

 private:

//append()���õĺ���
  size_t write(const char* logline, size_t len)
  {
#undef fwrite_unlocked
//������д�룬Ч�ʸ���
    return ::fwrite_unlocked(logline, 1, len, fp_);
  }

//�ļ�ָ��
  FILE* fp_;

  //�ļ�ָ��Ļ�����
  char buffer_[64*1024];

//�Ѿ�д����ֽ���
  size_t writtenBytes_;
};


//���캯��
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
//����basename��û��/
  assert(basename.find('/') == string::npos);

//���ʼ��fileָ�룬������־�ļ�
  rollFile();
}

LogFile::~LogFile()
{
}

//����־�ļ����������
void LogFile::append(const char* logline, int len)
{
//������̰߳�ȫ��
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

//ˢ���ļ�ָ��Ļ�����
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


//append()����
void LogFile::append_unlocked(const char* logline, int len)
{
//����־�ļ����������
  file_->append(logline, len);

//����Ѿ�д����ֽ�������Ҫ��ʼ�������ļ��Ĵ�С
  if (file_->writtenBytes() > rollSize_)
  {
  //������־
    rollFile();
  }
  else
  {
  
    if (count_ > kCheckTimeRoll_)
    {
      count_ = 0;
      time_t now = ::time(NULL);
	  
      time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
	  //���ʱ�����һ��,�������־�ļ�
      if (thisPeriod_ != startOfPeriod_)
      {
        rollFile();
      }
	  //���ˢ����ʱ����������flushInterval_,��ˢ�»�����
      else if (now - lastFlush_ > flushInterval_)
      {
        lastFlush_ = now;
        file_->flush();
      }
    }
    else
    {
    //����ֵ+1
      ++count_;
    }
  }
}

//������־�ļ�
void LogFile::rollFile()
{
  time_t now = 0;
  //��ȡ��־�ļ���,now������������Ǵ����ļ�����ʱ��
  string filename = getLogFileName(basename_, &now);

//start��һ���������������,Ҳ���ǽ���־�ļ�����ʱ�������������㣬Ϊ�˷���
  time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

//���now> �ϴι�����־��ʱ��
  if (now > lastRoll_)
  {
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = start;
	//����file_ָ��,������һ���µ��ļ�
	file_.reset(new File(filename));
  }
}

//��ȡ��־�ļ�����,now���������
string LogFile::getLogFileName(const string& basename, time_t* now)
{
  string filename;
  //�������Ŀռ�
  filename.reserve(basename.size() + 64);

  filename = basename;

  char timebuf[32];
  char pidbuf[32];
  struct tm tm;
  //��ȡ��ǰʱ�䣬������1970.....������
  *now = time(NULL);

  //���������õ�������ʱ����,���浽�ṹ����
  gmtime_r(now, &tm); // FIXME: localtime_r ?

//��ʽ��ʱ�䣬�����timebuf��
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm);

  filename += timebuf;
  
  filename += ProcessInfo::hostname();

//���̺�
  snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());

  filename += pidbuf;

  filename += ".log";

  return filename;
}

