#ifndef MUDUO_BASE_LOGSTREAM_H
#define MUDUO_BASE_LOGSTREAM_H

#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <assert.h>
#include <string.h> // memcpy
#ifndef MUDUO_STD_STRING
#include <string>
#endif
#include <boost/noncopyable.hpp>

namespace muduo
{

namespace detail
{

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000*1000;

//SIZE 是非类型参数，而是一个int类型的值
//日志输出的缓冲区类的封装
template<int SIZE>
class FixedBuffer : boost::noncopyable
{
 public:
  FixedBuffer()
    : cur_(data_)
  {
    setCookie(cookieStart);
  }

  ~FixedBuffer()
  {
    setCookie(cookieEnd);
  }

//将buf中数据添加到缓冲区中
  void append(const char* /*restrict*/ buf, size_t len)
  {
    // FIXME: append partially
    //如果缓冲区中的可用的字节数>len
    if (implicit_cast<size_t>(avail()) > len)
    {
    //追加到缓冲区中
      memcpy(cur_, buf, len);
      cur_ += len;
    }
  }

  const char* data() const { return data_; }

  //已写的缓存区的长度
  int length() const { return static_cast<int>(cur_ - data_); }

  // write to data_ directly
  //返回开始写的位置
  char* current() { return cur_; }

  //当前缓冲区可用的空间
  int avail() const { return static_cast<int>(end() - cur_); }

//增加
  void add(size_t len) { cur_ += len; }

//重置缓冲区空间
  void reset() { cur_ = data_; }

//将缓冲区数据置为0
  void bzero() { ::bzero(data_, sizeof data_); }

  // for used by GDB
  //将缓冲区数据的最后一个的下一个置为0,即转换成c风格的字符串
  const char* debugString();
  
  void setCookie(void (*cookie)()) { cookie_ = cookie; }

  // for used by unit test
  //将缓冲区中数据构造为string对象返回
  string asString() const { return string(data_, length()); }

 private:
 	//返回data_最后一个位置的下一个位置
  const char* end() const { return data_ + sizeof data_; }

  // Must be outline function for cookies.
  static void cookieStart();
  
  static void cookieEnd();

//函数指针，实际上并没有什么用
  void (*cookie_)();
  
  //缓冲区
  char data_[SIZE];

  //cur指向data_中写入的最后一个字符的下一个位置
  char* cur_;
};

}


class LogStream : boost::noncopyable
{
  typedef LogStream self;
 public:
 	//重定义FixedBuffer,指定缓冲区大小
  typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;

//重载<<操作符
  self& operator<<(bool v)
  {
  //将数据添加到缓冲区中
    buffer_.append(v ? "1" : "0", 1);
    return *this;
  }

  self& operator<<(short);
  self& operator<<(unsigned short);
  self& operator<<(int);
  self& operator<<(unsigned int);
  self& operator<<(long);
  self& operator<<(unsigned long);
  self& operator<<(long long);
  self& operator<<(unsigned long long);

  self& operator<<(const void*);

  self& operator<<(float v)
  {
    *this << static_cast<double>(v);
    return *this;
  }
  self& operator<<(double);
  // self& operator<<(long double);

  self& operator<<(char v)
  {
    buffer_.append(&v, 1);
    return *this;
  }

  // self& operator<<(signed char);
  // self& operator<<(unsigned char);

  self& operator<<(const char* v)
  {
    buffer_.append(v, strlen(v));
    return *this;
  }

  self& operator<<(const string& v)
  {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }

#ifndef MUDUO_STD_STRING
  self& operator<<(const std::string& v)
  {
    buffer_.append(v.c_str(), v.size());
    return *this;
  }
#endif

  self& operator<<(const StringPiece& v)
  {
    buffer_.append(v.data(), v.size());
    return *this;
  }

//往buffer中追加数据
  void append(const char* data, int len) { buffer_.append(data, len); }

//获取buffer
  const Buffer& buffer() const { return buffer_; }

//重置buffer
  void resetBuffer() { buffer_.reset(); }

 private:
  void staticCheck();

  template<typename T>
  void formatInteger(T);

//缓冲区对象
  Buffer buffer_;

  static const int kMaxNumericSize = 32;
};

//格式化的类
class Fmt // : boost::noncopyable
{
 public:
 	//构造函数
 	//将T按照fmt的格式输出到buf_缓冲区中
  template<typename T>
  Fmt(const char* fmt, T val);

//获取缓冲区地址
  const char* data() const { return buf_; }

  //获取数据长度
  int length() const { return length_; }

 private:
  char buf_[32];
  int length_;
};

inline LogStream& operator<<(LogStream& s, const Fmt& fmt)
{
  s.append(fmt.data(), fmt.length());
  return s;
}

}
#endif  // MUDUO_BASE_LOGSTREAM_H

