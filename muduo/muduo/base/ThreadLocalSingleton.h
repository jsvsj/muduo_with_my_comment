// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCALSINGLETON_H
#define MUDUO_BASE_THREADLOCALSINGLETON_H

#include <boost/noncopyable.hpp>
#include <assert.h>
#include <pthread.h>

namespace muduo
{

template<typename T>
	
//线程单例类，可以创建每个线程都有的单例对象,线程之间互不影响，即每个线程都有有单例T,要在全局作用域定义,
class ThreadLocalSingleton : boost::noncopyable
{
 public:

//获取*t_value_的引用,若不存在，则会创建T
  static T& instance()
  {
    if (!t_value_)
    {
      t_value_ = new T();
      deleter_.set(t_value_);
    }
    return *t_value_;
  }

//获取T的指针
  static T* pointer()
  {
    return t_value_;
  }

 private:

//销毁对象
  static void destructor(void* obj)
  {
    assert(obj == t_value_);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    delete t_value_;
    t_value_ = 0;
  }

//嵌套类,将Deleter中的key指向&T
  class Deleter
  {
   public:
   	//构造函数
    Deleter()
    {
    //创建一个key,在销毁时调用destructor()销毁key所对应的值
      pthread_key_create(&pkey_, &ThreadLocalSingleton::destructor);
    }

//析构函数，销毁key
    ~Deleter()
    {
      pthread_key_delete(pkey_);
    }
//设置key所对应的值
    void set(T* newObj)
    {
      assert(pthread_getspecific(pkey_) == NULL);
      pthread_setspecific(pkey_, newObj);
    }

    pthread_key_t pkey_;
  };

//__thread修饰，表示每个线程都都有一个
  static __thread T* t_value_;

  static Deleter deleter_;
};

template<typename T>
__thread T* ThreadLocalSingleton<T>::t_value_ = 0;

template<typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::deleter_;

}
#endif
