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

//两个buffer交换数据
  void swap(Buffer& rhs)
  {
    buffer_.swap(rhs.buffer_);
    std::swap(readerIndex_, rhs.readerIndex_);
    std::swap(writerIndex_, rhs.writerIndex_);
  }

//vector中可以读的数据的字节数
  size_t readableBytes() const
  { return writerIndex_ - readerIndex_; }

//vector中可以写的空间的字节数
  size_t writableBytes() const
  { return buffer_.size() - writerIndex_; }

//返回vector中预留的位置的字节数 
  size_t prependableBytes() const
  { return readerIndex_; }

 //返回读开始的指针
  const char* peek() const
  { return begin() + readerIndex_; }

//从度开始的位置在读的数据中查找\r\n
  const char* findCRLF() const
  {
    const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
    return crlf == beginWrite() ? NULL : crlf;
  }

//从指定位置在读的数据中查找\r\n
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

  //将读开始的位置向后移动len个字节
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

//将开始读的位置移动到直到end位置
  void retrieveUntil(const char* end)
  {
    assert(peek() <= end);
    assert(end <= beginWrite());
    retrieve(end - peek());
  }

//将开始度的位置移动4个字节
  void retrieveInt32()
  {
    retrieve(sizeof(int32_t));
  }

//将开始度的位置移动2个字节

  void retrieveInt16()
  {
    retrieve(sizeof(int16_t));
  }

//将开始度的位置移动1个字节
  void retrieveInt8()
  {
    retrieve(sizeof(int8_t));
  }

//将下标位置重置，在读取玩所有数据后执行此操作
  void retrieveAll()
  {
    readerIndex_ = kCheapPrepend;
    writerIndex_ = kCheapPrepend;
  }

//取回所有字节，并将其变为strin对象返回
  string retrieveAllAsString()
  {
    return retrieveAsString(readableBytes());;
  }

//将可读的数据段从开始位置的len个字节封装string对象
  string retrieveAsString(size_t len)
  {
    assert(len <= readableBytes());
    string result(peek(), len);
    retrieve(len);
    return result;
  }

//将所有可读数据构造成StringPeace对象返回
  StringPiece toStringPiece() const
  {
    return StringPiece(peek(), static_cast<int>(readableBytes()));
  }

//往缓冲区中写入一个StringPiece对象
  void append(const StringPiece& str)
  {
    append(str.data(), str.size());
  }

//往缓冲区中写入data，
  void append(const char* /*restrict*/ data, size_t len)
  {
    ensureWritableBytes(len);
    std::copy(data, data+len, beginWrite());
    hasWritten(len);
  }

  //往缓冲区中写入data，
  void append(const void* /*restrict*/ data, size_t len)
  {
    append(static_cast<const char*>(data), len);
  }

//确保缓冲区中还能写入len个字节,不足的话就要调用makeSpace(len)腾出空间
  void ensureWritableBytes(size_t len)
  {
    if (writableBytes() < len)
    {
      makeSpace(len);
    }
    assert(writableBytes() >= len);
  }

//返回开始写的在vector中的位置指针
  char* beginWrite()
  { return begin() + writerIndex_; }

  const char* beginWrite() const
  { return begin() + writerIndex_; }

//往缓冲区中写入了len个字节后，写开始的位置+len
  void hasWritten(size_t len)
  { writerIndex_ += len; }

  ///
  /// Append int32_t using network endian
  ///
  //写入4个字节
  void appendInt32(int32_t x)
  {
    int32_t be32 = sockets::hostToNetwork32(x);
    append(&be32, sizeof be32);
  }

//写入2个字节 
  void appendInt16(int16_t x)
  {
    int16_t be16 = sockets::hostToNetwork16(x);
    append(&be16, sizeof be16);
  }

//写入1个字节
  void appendInt8(int8_t x)
  {
    append(&x, sizeof x);
  }

  ///
  /// Read int32_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  //读取4个字节
  int32_t readInt32()
  {
    int32_t result = peekInt32();
    retrieveInt32();
    return result;
  }

  //读取2个字节
  int16_t readInt16()
  {
    int16_t result = peekInt16();
    retrieveInt16();
    return result;
  }
  
  //读取1个字节
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
  
  //读取4个字节，被对应函数调用
  int32_t peekInt32() const
  {
    assert(readableBytes() >= sizeof(int32_t));
    int32_t be32 = 0;
    ::memcpy(&be32, peek(), sizeof be32);
    return sockets::networkToHost32(be32);
  }

//读取2个字节,被对应函数调用
  int16_t peekInt16() const
  {
    assert(readableBytes() >= sizeof(int16_t));
    int16_t be16 = 0;
    ::memcpy(&be16, peek(), sizeof be16);
    return sockets::networkToHost16(be16);
  }

//读取1个字节，被对应函数调用
  int8_t peekInt8() const
  {
    assert(readableBytes() >= sizeof(int8_t));
    int8_t x = *peek();
    return x;
  }

  ///
  /// Prepend int32_t using network endian
  ///

  //在预留区域中添加4个字节
  void prependInt32(int32_t x)
  {
    int32_t be32 = sockets::hostToNetwork32(x);
    prepend(&be32, sizeof be32);
  }

//在预留区域中添加2个字节,
  void prependInt16(int16_t x)
  {
    int16_t be16 = sockets::hostToNetwork16(x);
    prepend(&be16, sizeof be16);
  }

//在预留区域中添加1个字节
  void prependInt8(int8_t x)
  {
    prepend(&x, sizeof x);
  }

//在预留区域中添加len个字节 
  void prepend(const void* /*restrict*/ data, size_t len)
  {
    assert(len <= prependableBytes());
    readerIndex_ -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d, d+len, begin()+readerIndex_);
  }

//伸缩空间，保留reserve个字节
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
  
  //从套接字中读取，添加到缓冲区中
  ssize_t readFd(int fd, int* savedErrno);

 private:

//返回vector的头位置
  char* begin()
  { return &*buffer_.begin(); }

  const char* begin() const
  { return &*buffer_.begin(); }


//腾出空间：扩充空间或者数据块的移动
  void makeSpace(size_t len)
  {
  //如果腾出空间还是不能容纳要写入的数据，则扩容
    if (writableBytes() + prependableBytes() < len + kCheapPrepend)
    {
      // FIXME: move readable data
      buffer_.resize(writerIndex_+len);
    }
	//将数据块移动，腾出空间 
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
 	//vector<char>用于替代长度固定的数组
  std::vector<char> buffer_;

//像是一个队列

	//读位置
  size_t readerIndex_;

	//写位置
  size_t writerIndex_;

  static const char kCRLF[];
};

}
}

#endif  // MUDUO_NET_BUFFER_H
