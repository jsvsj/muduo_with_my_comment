// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/base/FileUtil.h>

#include <boost/static_assert.hpp>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace muduo;

//构造函数
FileUtil::SmallFile::SmallFile(StringPiece filename)
	//打开文件
	: fd_(::open(filename.data(), O_RDONLY | O_CLOEXEC)),
    err_(0)
{
  buf_[0] = '\0';
  if (fd_ < 0)
  {
    err_ = errno;
  }
}


//析构函数,关闭文件描述符
FileUtil::SmallFile::~SmallFile()
{
  if (fd_ >= 0)
  {
    ::close(fd_); // FIXME: check EINTR
  }
}

//将文件内容读取到string类型中,maxsize为能读取的最大长度,fileSize,modifyTime,createTime为传出参数
// return errno
template<typename String>
int FileUtil::SmallFile::readToString(int maxSize,
                                      String* content,
                                      int64_t* fileSize,
                                      int64_t* modifyTime,
                                      int64_t* createTime)
{
  BOOST_STATIC_ASSERT(sizeof(off_t) == 8);
  assert(content != NULL);
  int err = err_;
  if (fd_ >= 0)
  {
  //清空
    content->clear();
//如果不为空
    if (fileSize)
    {
    //获取文件的属性
      struct stat statbuf;
      if (::fstat(fd_, &statbuf) == 0)
      {
      //如果是普通文件
        if (S_ISREG(statbuf.st_mode))
        {
        //文件大小
          *fileSize = statbuf.st_size;
		//设置string的大小
		  content->reserve(static_cast<int>(std::min(implicit_cast<int64_t>(maxSize), *fileSize)));
        }
		//如果是目录
        else if (S_ISDIR(statbuf.st_mode))
        {
          err = EISDIR;
        }
        if (modifyTime)
        {
        //获取文件属性
          *modifyTime = statbuf.st_mtime;
        }
        if (createTime)
        {
        //获取文件创建时间
          *createTime = statbuf.st_ctime;
        }
      }
      else
      {
        err = errno;
      }
    }

    while (content->size() < implicit_cast<size_t>(maxSize))
    {
    //toread要读取的字节数
      size_t toRead = std::min(implicit_cast<size_t>(maxSize) - content->size(), sizeof(buf_));

	  ssize_t n = ::read(fd_, buf_, toRead);

	  if (n > 0)
      {
        content->append(buf_, n);
      }
      else
      {
        if (n < 0)
        {
          err = errno;
        }
		//如果n==0，则跳出循环
        break;
      }
    }
  }
  return err;
}

//读到char []缓冲区中
//size传出参数。表示读到的字节数
int FileUtil::SmallFile::readToBuffer(int* size)
{
  int err = err_;
  if (fd_ >= 0)
  {
    ssize_t n = ::pread(fd_, buf_, sizeof(buf_)-1, 0);
    if (n >= 0)
    {
      if (size)
      {
        *size = static_cast<int>(n);
      }
      buf_[n] = '\0';
    }
    else
    {
      err = errno;
    }
  }
  return err;
}

template int FileUtil::readFile(StringPiece filename,
                                int maxSize,
                                string* content,
                                int64_t*, int64_t*, int64_t*);

template int FileUtil::SmallFile::readToString(
    int maxSize,
    string* content,
    int64_t*, int64_t*, int64_t*);

#ifndef MUDUO_STD_STRING
template int FileUtil::readFile(StringPiece filename,
                                int maxSize,
                                std::string* content,
                                int64_t*, int64_t*, int64_t*);

template int FileUtil::SmallFile::readToString(
    int maxSize,
    std::string* content,
    int64_t*, int64_t*, int64_t*);
#endif

