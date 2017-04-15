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
	
//�̵߳����࣬���Դ���ÿ���̶߳��еĵ�������,�߳�֮�以��Ӱ�죬��ÿ���̶߳����е���T,Ҫ��ȫ����������,
class ThreadLocalSingleton : boost::noncopyable
{
 public:

//��ȡ*t_value_������,�������ڣ���ᴴ��T
  static T& instance()
  {
    if (!t_value_)
    {
      t_value_ = new T();
      deleter_.set(t_value_);
    }
    return *t_value_;
  }

//��ȡT��ָ��
  static T* pointer()
  {
    return t_value_;
  }

 private:

//���ٶ���
  static void destructor(void* obj)
  {
    assert(obj == t_value_);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    delete t_value_;
    t_value_ = 0;
  }

//Ƕ����,��Deleter�е�keyָ��&T
  class Deleter
  {
   public:
   	//���캯��
    Deleter()
    {
    //����һ��key,������ʱ����destructor()����key����Ӧ��ֵ
      pthread_key_create(&pkey_, &ThreadLocalSingleton::destructor);
    }

//��������������key
    ~Deleter()
    {
      pthread_key_delete(pkey_);
    }
//����key����Ӧ��ֵ
    void set(T* newObj)
    {
      assert(pthread_getspecific(pkey_) == NULL);
      pthread_setspecific(pkey_, newObj);
    }

    pthread_key_t pkey_;
  };

//__thread���Σ���ʾÿ���̶߳�����һ��
  static __thread T* t_value_;

  static Deleter deleter_;
};

template<typename T>
__thread T* ThreadLocalSingleton<T>::t_value_ = 0;

template<typename T>
typename ThreadLocalSingleton<T>::Deleter ThreadLocalSingleton<T>::deleter_;

}
#endif
