#include <muduo/base/Timestamp.h>

#include <sys/time.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#undef __STDC_FORMAT_MACROS

#include <boost/static_assert.hpp>

using namespace muduo;


//assert:运行时断言
//BOOST_STATIC_ASSERT编译是断言
BOOST_STATIC_ASSERT(sizeof(Timestamp) == sizeof(int64_t));

//构造函数
Timestamp::Timestamp(int64_t microseconds)
  : microSecondsSinceEpoch_(microseconds)
{
}

//将时间转换成字符串
string Timestamp::toString() const
{
  char buf[32] = {0};
  //时间转换成秒数的整数部分
  int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;

//时间转换成秒数后的小数部分
  int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;

//PRId6432位和64位下  跨平台的做法
  snprintf(buf, sizeof(buf)-1, "%" PRId64 ".%06" PRId64 "", seconds, microseconds);

  return buf;
}

//将时间转换产格式化的字符串
string Timestamp::toFormattedString() const
{
  char buf[32] = {0};
  //秒数整数部分
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);

  int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
//秒数小数部分

//定义tm结构体
  struct tm tm_time;

//将描述部分转换成结构体中相应的时间格式，如年，月，日
  gmtime_r(&seconds, &tm_time);

  snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
      tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
      tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
      microseconds);
  
  return buf;
}

//static修饰 
//返回一个Timestamp对象，其内部的时间为当前时间与1970年1月1日0点的时间间隔的微秒数
Timestamp Timestamp::now()
{
  struct timeval tv;

  /*
  struct timeval {	
	  time_t	  tv_sec;	 秒数
	  suseconds_t tv_usec;	 微秒
  };
*/

  //获取当前时间与1970年1月1日0点的时间间隔的微秒数
  gettimeofday(&tv, NULL);

   //秒数
  int64_t seconds = tv.tv_sec;
 
  return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

//static修饰
//获取一个microSecondsSinceEpoch_=0的时间戳对象,错误的时间
Timestamp Timestamp::invalid()
{
  return Timestamp();
}

