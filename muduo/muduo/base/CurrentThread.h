// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CURRENTTHREAD_H
#define MUDUO_BASE_CURRENTTHREAD_H

namespace muduo
{
namespace CurrentThread
{

//变量的声明
  // internal
  //__thread修饰，线程范围的全局变量，每个线程都有一个
  extern __thread int t_cachedTid;

  //tid的字符串表现形式
  extern __thread char t_tidString[32];
  
  //线程的名称
  extern __thread const char* t_threadName;
  void cacheTid();

//获取线程的tid
  inline int tid()
  {
  //如果还没有缓存过
    if (t_cachedTid == 0)
    {
    //缓存
      cacheTid();
    }
    return t_cachedTid;
  }


  inline const char* tidString() // for logging
  {
    return t_tidString;
  }

  inline const char* name()
  {
    return t_threadName;
  }

  bool isMainThread();
}
}

#endif
