// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_BUFFER_H
#define MUDUO_NET_BUFFER_H

#include <muduo/base/copyable.h>
#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>

#include <muduo/net/Endian.h>

#include <algorithm>
#include <vector>

#include <assert.h>
#include <string.h>
//#include <unistd.h>  // ssize_t

namespace muduo
{
namespace net
{

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
class Buffer : public muduo::copyable
{
 public:
  static const size_t kCheapPrepend = 8;
  static const size_t kInitialSize = 1024;

  Buffer()
    : buffer_(kCheapPrepend + kInitialSize),
      readerIndex_(kCheapPrepend),
      writerIndex_(kCheapPrepend)
  {
    assert(readableBytes() == 0);
    assert(writableBytes() == kInitialSize);
    assert(prependableBytes() == kCheapPrepend);
  }

  // default copy-ctor, dtor and assignment are fine

//����buffer��������
  void swap(Buffer& rhs)
  {
    buffer_.swap(rhs.buffer_);
    std::swap(readerIndex_, rhs.readerIndex_);
    std::swap(writerIndex_, rhs.writerIndex_);
  }

//vector�п��Զ������ݵ��ֽ���
  size_t readableBytes() const
  { return writerIndex_ - readerIndex_; }

//vector�п���д�Ŀռ���ֽ���
  size_t writableBytes() const
  { return buffer_.size() - writerIndex_; }

//����vector��Ԥ����λ�õ��ֽ��� 
  size_t prependableBytes() const
  { return readerIndex_; }

 //���ض���ʼ��ָ��
  const char* peek() const
  { return begin() + readerIndex_; }

//�Ӷȿ�ʼ��λ���ڶ��������в���\r\n
  const char* findCRLF() const
  {
    const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
    return crlf == beginWrite() ? NULL : crlf;
  }

//��ָ��λ���ڶ��������в���\r\n
  const char* findCRLF(const char* start) const
  {
    assert(peek() <= start);
    assert(start <= beginWrite());
    const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
    return crlf == beginWrite() ? NULL : crlf;
  }

  // retrieve returns void, to prevent
  // string str(retrieve(readableBytes()), readableBytes());
  // the evaluation of two functions are unspecified

  //������ʼ��λ������ƶ�len���ֽ�
  void retrieve(size_t len)
  {
    assert(len <= readableBytes());
    if (len < readableBytes())
    {
      readerIndex_ += len;
    }
    else
    {
      retrieveAll();
    }
  }

//����ʼ����λ���ƶ���ֱ��endλ��
  void retrieveUntil(const char* end)
  {
    assert(peek() <= end);
    assert(end <= beginWrite());
    retrieve(end - peek());
  }

//����ʼ�ȵ�λ���ƶ�4���ֽ�
  void retrieveInt32()
  {
    retrieve(sizeof(int32_t));
  }

//����ʼ�ȵ�λ���ƶ�2���ֽ�

  void retrieveInt16()
  {
    retrieve(sizeof(int16_t));
  }

//����ʼ�ȵ�λ���ƶ�1���ֽ�
  void retrieveInt8()
  {
    retrieve(sizeof(int8_t));
  }

//���±�λ�����ã��ڶ�ȡ���������ݺ�ִ�д˲���
  void retrieveAll()
  {
    readerIndex_ = kCheapPrepend;
    writerIndex_ = kCheapPrepend;
  }

//ȡ�������ֽڣ��������Ϊstrin���󷵻�
  string retrieveAllAsString()
  {
    return retrieveAsString(readableBytes());;
  }

//���ɶ������ݶδӿ�ʼλ�õ�len���ֽڷ�װstring����
  string retrieveAsString(size_t len)
  {
    assert(len <= readableBytes());
    string result(peek(), len);
    retrieve(len);
    return result;
  }

//�����пɶ����ݹ����StringPeace���󷵻�
  StringPiece toStringPiece() const
  {
    return StringPiece(peek(), static_cast<int>(readableBytes()));
  }

//����������д��һ��StringPiece����
  void append(const StringPiece& str)
  {
    append(str.data(), str.size());
  }

//����������д��data��
  void append(const char* /*restrict*/ data, size_t len)
  {
    ensureWritableBytes(len);
    std::copy(data, data+len, beginWrite());
    hasWritten(len);
  }

  //����������д��data��
  void append(const void* /*restrict*/ data, size_t len)
  {
    append(static_cast<const char*>(data), len);
  }

//ȷ���������л���д��len���ֽ�,����Ļ���Ҫ����makeSpace(len)�ڳ��ռ�
  void ensureWritableBytes(size_t len)
  {
    if (writableBytes() < len)
    {
      makeSpace(len);
    }
    assert(writableBytes() >= len);
  }

//���ؿ�ʼд����vector�е�λ��ָ��
  char* beginWrite()
  { return begin() + writerIndex_; }

  const char* beginWrite() const
  { return begin() + writerIndex_; }

//����������д����len���ֽں�д��ʼ��λ��+len
  void hasWritten(size_t len)
  { writerIndex_ += len; }

  ///
  /// Append int32_t using network endian
  ///
  //д��4���ֽ�
  void appendInt32(int32_t x)
  {
    int32_t be32 = sockets::hostToNetwork32(x);
    append(&be32, sizeof be32);
  }

//д��2���ֽ� 
  void appendInt16(int16_t x)
  {
    int16_t be16 = sockets::hostToNetwork16(x);
    append(&be16, sizeof be16);
  }

//д��1���ֽ�
  void appendInt8(int8_t x)
  {
    append(&x, sizeof x);
  }

  ///
  /// Read int32_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  //��ȡ4���ֽ�
  int32_t readInt32()
  {
    int32_t result = peekInt32();
    retrieveInt32();
    return result;
  }

  //��ȡ2���ֽ�
  int16_t readInt16()
  {
    int16_t result = peekInt16();
    retrieveInt16();
    return result;
  }
  
  //��ȡ1���ֽ�
  int8_t readInt8()
  {
    int8_t result = peekInt8();
    retrieveInt8();
    return result;
  }

  ///
  /// Peek int32_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  
  //��ȡ4���ֽڣ�����Ӧ��������
  int32_t peekInt32() const
  {
    assert(readableBytes() >= sizeof(int32_t));
    int32_t be32 = 0;
    ::memcpy(&be32, peek(), sizeof be32);
    return sockets::networkToHost32(be32);
  }

//��ȡ2���ֽ�,����Ӧ��������
  int16_t peekInt16() const
  {
    assert(readableBytes() >= sizeof(int16_t));
    int16_t be16 = 0;
    ::memcpy(&be16, peek(), sizeof be16);
    return sockets::networkToHost16(be16);
  }

//��ȡ1���ֽڣ�����Ӧ��������
  int8_t peekInt8() const
  {
    assert(readableBytes() >= sizeof(int8_t));
    int8_t x = *peek();
    return x;
  }

  ///
  /// Prepend int32_t using network endian
  ///

  //��Ԥ�����������4���ֽ�
  void prependInt32(int32_t x)
  {
    int32_t be32 = sockets::hostToNetwork32(x);
    prepend(&be32, sizeof be32);
  }

//��Ԥ�����������2���ֽ�,
  void prependInt16(int16_t x)
  {
    int16_t be16 = sockets::hostToNetwork16(x);
    prepend(&be16, sizeof be16);
  }

//��Ԥ�����������1���ֽ�
  void prependInt8(int8_t x)
  {
    prepend(&x, sizeof x);
  }

//��Ԥ�����������len���ֽ� 
  void prepend(const void* /*restrict*/ data, size_t len)
  {
    assert(len <= prependableBytes());
    readerIndex_ -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d, d+len, begin()+readerIndex_);
  }

//�����ռ䣬����reserve���ֽ�
  void shrink(size_t reserve)
  {
    // FIXME: use vector::shrink_to_fit() in C++ 11 if possible.
    Buffer other;
    other.ensureWritableBytes(readableBytes()+reserve);
    other.append(toStringPiece());
    swap(other);
  }

  /// Read data directly into buffer.
  ///
  /// It may implement with readv(2)
  /// @return result of read(2), @c errno is saved
  
  //���׽����ж�ȡ����ӵ���������
  ssize_t readFd(int fd, int* savedErrno);

 private:

//����vector��ͷλ��
  char* begin()
  { return &*buffer_.begin(); }

  const char* begin() const
  { return &*buffer_.begin(); }


//�ڳ��ռ䣺����ռ�������ݿ���ƶ�
  void makeSpace(size_t len)
  {
  //����ڳ��ռ仹�ǲ�������Ҫд������ݣ�������
    if (writableBytes() + prependableBytes() < len + kCheapPrepend)
    {
      // FIXME: move readable data
      buffer_.resize(writerIndex_+len);
    }
	//�����ݿ��ƶ����ڳ��ռ� 
    else
    {
      // move readable data to the front, make space inside buffer
      assert(kCheapPrepend < readerIndex_);
      size_t readable = readableBytes();
      std::copy(begin()+readerIndex_,
                begin()+writerIndex_,
                begin()+kCheapPrepend);
      readerIndex_ = kCheapPrepend;
      writerIndex_ = readerIndex_ + readable;
      assert(readable == readableBytes());
    }
  }

 private:
 	//vector<char>����������ȹ̶�������
  std::vector<char> buffer_;

//����һ������

	//��λ��
  size_t readerIndex_;

	//дλ��
  size_t writerIndex_;

  static const char kCRLF[];
};

}
}

#endif  // MUDUO_NET_BUFFER_H
