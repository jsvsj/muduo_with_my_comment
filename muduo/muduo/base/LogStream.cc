#include <muduo/base/LogStream.h>

#include <algorithm>
#include <limits>
#include <boost/static_assert.hpp>
#include <boost/type_traits/is_arithmetic.hpp>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

using namespace muduo;
using namespace muduo::detail;

#pragma GCC diagnostic ignored "-Wtype-limits"
//#pragma GCC diagnostic error "-Wtype-limits"
namespace muduo
{
namespace detail
{

const char digits[] = "9876543210123456789";
const char* zero = digits + 9;
BOOST_STATIC_ASSERT(sizeof(digits) == 20);

const char digitsHex[] = "0123456789ABCDEF";
BOOST_STATIC_ASSERT(sizeof digitsHex == 17);

// Efficient Integer to String Conversions, by Matthew Wilson.
//将十进制数字转换成字符存入buf中
template<typename T>
size_t convert(char buf[], T value)
{
  T i = value;
  char* p = buf;

  do
  {
    int lsd = static_cast<int>(i % 10);
    i /= 10;
	//将余数对应的字符存入buf中
    *p++ = zero[lsd];
  } while (i != 0);

//如果是负数
  if (value < 0)
  {
    *p++ = '-';
  }
  *p = '\0';

  //倒转
  std::reverse(buf, p);

//返回长度
  return p - buf;
}

//将16进制数转换成字符
size_t convertHex(char buf[], uintptr_t value)
{
  uintptr_t i = value;
  char* p = buf;

  do
  {
    int lsd = i % 16;
    i /= 16;
    *p++ = digitsHex[lsd];
  } while (i != 0);

  *p = '\0';
  std::reverse(buf, p);

  return p - buf;
}

}
}

template<int SIZE>
//将缓冲区数据的最后一个的下一个置为0,即转换成c风格的字符串
const char* FixedBuffer<SIZE>::debugString()
{
  *cur_ = '\0';
  return data_;
}

template<int SIZE>
void FixedBuffer<SIZE>::cookieStart()
{
}

template<int SIZE>
void FixedBuffer<SIZE>::cookieEnd()
{
}

template class FixedBuffer<kSmallBuffer>;
template class FixedBuffer<kLargeBuffer>;

void LogStream::staticCheck()
{
  BOOST_STATIC_ASSERT(kMaxNumericSize - 10 > std::numeric_limits<double>::digits10);
  BOOST_STATIC_ASSERT(kMaxNumericSize - 10 > std::numeric_limits<long double>::digits10);
  BOOST_STATIC_ASSERT(kMaxNumericSize - 10 > std::numeric_limits<long>::digits10);
  BOOST_STATIC_ASSERT(kMaxNumericSize - 10 > std::numeric_limits<long long>::digits10);
}

//将数字转换成字符串加入到缓冲区中
template<typename T>
void LogStream::formatInteger(T v)
{
  if (buffer_.avail() >= kMaxNumericSize)
  {
  //将v转换成字符追加到buffer_中
    size_t len = convert(buffer_.current(), v);
  //移动cur指针
    buffer_.add(len);
  }
}

//重载<<操作符
LogStream& LogStream::operator<<(short v)
{
  *this << static_cast<int>(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned short v)
{
  *this << static_cast<unsigned int>(v);
  return *this;
}

LogStream& LogStream::operator<<(int v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned int v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(long v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned long v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(long long v)
{
  formatInteger(v);
  return *this;
}

LogStream& LogStream::operator<<(unsigned long long v)
{
  formatInteger(v);
  return *this;
}

//对于void *p来说，是将其16进制的地址存入到缓冲区中
LogStream& LogStream::operator<<(const void* p)
{
//uintptr_t
//对于32位平台来说就是unsigned int
//对于64位平台来说就是unsigned long int
  uintptr_t v = reinterpret_cast<uintptr_t>(p);

  if (buffer_.avail() >= kMaxNumericSize)
  {
    char* buf = buffer_.current();
    buf[0] = '0';
    buf[1] = 'x';

    size_t len = convertHex(buf+2, v);

    buffer_.add(len+2);
  }

  return *this;
}

// FIXME: replace this with Grisu3 by Florian Loitsch.
LogStream& LogStream::operator<<(double v)
{
  if (buffer_.avail() >= kMaxNumericSize)
  {
    int len = snprintf(buffer_.current(), kMaxNumericSize, "%.12g", v);
    buffer_.add(len);
  }
  return *this;
}

//类Fmt的构造函数
template<typename T>
Fmt::Fmt(const char* fmt, T val)
{
  BOOST_STATIC_ASSERT(boost::is_arithmetic<T>::value == true);
//按照fmt所指的格式将val输出到buf_中
  length_ = snprintf(buf_, sizeof buf_, fmt, val);

  assert(static_cast<size_t>(length_) < sizeof buf_);
}

// Explicit instantiations

template Fmt::Fmt(const char* fmt, char);

template Fmt::Fmt(const char* fmt, short);
template Fmt::Fmt(const char* fmt, unsigned short);
template Fmt::Fmt(const char* fmt, int);
template Fmt::Fmt(const char* fmt, unsigned int);
template Fmt::Fmt(const char* fmt, long);
template Fmt::Fmt(const char* fmt, unsigned long);
template Fmt::Fmt(const char* fmt, long long);
template Fmt::Fmt(const char* fmt, unsigned long long);

template Fmt::Fmt(const char* fmt, float);
template Fmt::Fmt(const char* fmt, double);
