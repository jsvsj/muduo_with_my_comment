// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include <boost/noncopyable.hpp>
#include <pthread.h>
#include <stdlib.h> // atexit

namespace muduo
{

//线程安全的单例类
template<typename T>

class Singleton : boost::noncopyable
{
 public:
 	//获取对象
  static T& instance()
  {
  //pthread_once()只会调用一次
    pthread_once(&ponce_, &Singleton::init);
    return *value_;
  }

 private:
 	//私有构造，析构
  Singleton();
  ~Singleton();

//创建一个对象
  static void init()
  {
    value_ = new T();
	//注册一个销毁函数
    ::atexit(destroy);
  }
//销毁该创建的对象
  static void destroy()
  {
  //保证T是完整的类型，而不是类的声明
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
	
    delete value_;
  }

 private:
  static pthread_once_t ponce_;
  static T*             value_;
};

template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template<typename T>
T* Singleton<T>::value_ = NULL;

}
#endif

