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

//�̰߳�ȫ�ĵ�����
template<typename T>

class Singleton : boost::noncopyable
{
 public:
 	//��ȡ����
  static T& instance()
  {
  //pthread_once()ֻ�����һ��
    pthread_once(&ponce_, &Singleton::init);
    return *value_;
  }

 private:
 	//˽�й��죬����
  Singleton();
  ~Singleton();

//����һ������
  static void init()
  {
    value_ = new T();
	//ע��һ�����ٺ���
    ::atexit(destroy);
  }
//���ٸô����Ķ���
  static void destroy()
  {
  //��֤T�����������ͣ��������������
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

