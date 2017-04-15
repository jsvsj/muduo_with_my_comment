// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCAL_H
#define MUDUO_BASE_THREADLOCAL_H

#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo
{

template<typename T>

//线程特定的全局变量，对于PLD类型可以用__thread,每个线程都有
class ThreadLocal : boost::noncopyable
{
 public:
 	//构造函数
  ThreadLocal()
  {
  //创建一个key,其中destructor指定了key销毁时的回调函数
    pthread_key_create(&pkey_, &ThreadLocal::destructor);
  }

//析构函数。销毁key,同时会调用destructor销毁key指向的值
  ~ThreadLocal()
  {
    pthread_key_delete(pkey_);
  }

//获取线程特定数据
  T& value()
  {
 	//调用函数pthread_getspecific(key)获取key指向的数据 
    T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));
//如果数据为空,则创建并设置
	if (!perThreadValue) {
		
      T* newObj = new T();
      pthread_setspecific(pkey_, newObj);
      perThreadValue = newObj;
    }
    return *perThreadValue;
  }

 private:
//销毁数据函数，static是为了符合pthread_key_create()中回调函数做参数的类型，不包含隐含的this指针
  static void destructor(void *x)
  {
    T* obj = static_cast<T*>(x);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    delete obj;
  }

 private:
  pthread_key_t pkey_;
};

}
#endif
