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

//SIZE �Ƿ����Ͳ���������һ��int���͵�ֵ
//��־����Ļ�������ķ�װ
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

//��buf��������ӵ���������
  void append(const char* /*restrict*/ buf, size_t len)
  {
    // FIXME: append partially
    //����������еĿ��õ��ֽ���>len
    if (implicit_cast<size_t>(avail()) > len)
    {
    //׷�ӵ���������
      memcpy(cur_, buf, len);
      cur_ += len;
    }
  }

  const char* data() const { return data_; }

  //��д�Ļ������ĳ���
  int length() const { return static_cast<int>(cur_ - data_); }

  // write to data_ directly
  //���ؿ�ʼд��λ��
  char* current() { return cur_; }

  //��ǰ���������õĿռ�
  int avail() const { return static_cast<int>(end() - cur_); }

//����
  void add(size_t len) { cur_ += len; }

//���û������ռ�
  void reset() { cur_ = data_; }

//��������������Ϊ0
  void bzero() { ::bzero(data_, sizeof data_); }

  // for used by GDB
  //�����������ݵ����һ������һ����Ϊ0,��ת����c�����ַ���
  const char* debugString();
  
  void setCookie(void (*cookie)()) { cookie_ = cookie; }

  // for used by unit test
  //�������������ݹ���Ϊstring���󷵻�
  string asString() const { return string(data_, length()); }

 private:
 	//����data_���һ��λ�õ���һ��λ��
  const char* end() const { return data_ + sizeof data_; }

  // Must be outline function for cookies.
  static void cookieStart();
  
  static void cookieEnd();

//����ָ�룬ʵ���ϲ�û��ʲô��
  void (*cookie_)();
  
  //������
  char data_[SIZE];

  //curָ��data_��д������һ���ַ�����һ��λ��
  char* cur_;
};

}


class LogStream : boost::noncopyable
{
  typedef LogStream self;
 public:
 	//�ض���FixedBuffer,ָ����������С
  typedef detail::FixedBuffer<detail::kSmallBuffer> Buffer;

//����<<������
  self& operator<<(bool v)
  {
  //��������ӵ���������
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

//��buffer��׷������
  void append(const char* data, int len) { buffer_.append(data, len); }

//��ȡbuffer
  const Buffer& buffer() const { return buffer_; }

//����buffer
  void resetBuffer() { buffer_.reset(); }

 private:
  void staticCheck();

  template<typename T>
  void formatInteger(T);

//����������
  Buffer buffer_;

  static const int kMaxNumericSize = 32;
};

//��ʽ������
class Fmt // : boost::noncopyable
{
 public:
 	//���캯��
 	//��T����fmt�ĸ�ʽ�����buf_��������
  template<typename T>
  Fmt(const char* fmt, T val);

//��ȡ��������ַ
  const char* data() const { return buf_; }

  //��ȡ���ݳ���
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

