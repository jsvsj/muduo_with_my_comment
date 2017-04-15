#include <muduo/base/Timestamp.h>

#include <sys/time.h>
#include <stdio.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#undef __STDC_FORMAT_MACROS

#include <boost/static_assert.hpp>

using namespace muduo;


//assert:����ʱ����
//BOOST_STATIC_ASSERT�����Ƕ���
BOOST_STATIC_ASSERT(sizeof(Timestamp) == sizeof(int64_t));

//���캯��
Timestamp::Timestamp(int64_t microseconds)
  : microSecondsSinceEpoch_(microseconds)
{
}

//��ʱ��ת�����ַ���
string Timestamp::toString() const
{
  char buf[32] = {0};
  //ʱ��ת������������������
  int64_t seconds = microSecondsSinceEpoch_ / kMicroSecondsPerSecond;

//ʱ��ת�����������С������
  int64_t microseconds = microSecondsSinceEpoch_ % kMicroSecondsPerSecond;

//PRId6432λ��64λ��  ��ƽ̨������
  snprintf(buf, sizeof(buf)-1, "%" PRId64 ".%06" PRId64 "", seconds, microseconds);

  return buf;
}

//��ʱ��ת������ʽ�����ַ���
string Timestamp::toFormattedString() const
{
  char buf[32] = {0};
  //������������
  time_t seconds = static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);

  int microseconds = static_cast<int>(microSecondsSinceEpoch_ % kMicroSecondsPerSecond);
//����С������

//����tm�ṹ��
  struct tm tm_time;

//����������ת���ɽṹ������Ӧ��ʱ���ʽ�����꣬�£���
  gmtime_r(&seconds, &tm_time);

  snprintf(buf, sizeof(buf), "%4d%02d%02d %02d:%02d:%02d.%06d",
      tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
      tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
      microseconds);
  
  return buf;
}

//static���� 
//����һ��Timestamp�������ڲ���ʱ��Ϊ��ǰʱ����1970��1��1��0���ʱ������΢����
Timestamp Timestamp::now()
{
  struct timeval tv;

  /*
  struct timeval {	
	  time_t	  tv_sec;	 ����
	  suseconds_t tv_usec;	 ΢��
  };
*/

  //��ȡ��ǰʱ����1970��1��1��0���ʱ������΢����
  gettimeofday(&tv, NULL);

   //����
  int64_t seconds = tv.tv_sec;
 
  return Timestamp(seconds * kMicroSecondsPerSecond + tv.tv_usec);
}

//static����
//��ȡһ��microSecondsSinceEpoch_=0��ʱ�������,�����ʱ��
Timestamp Timestamp::invalid()
{
  return Timestamp();
}

