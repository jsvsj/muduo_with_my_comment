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

//�߳��ض���ȫ�ֱ���������PLD���Ϳ�����__thread,ÿ���̶߳���
class ThreadLocal : boost::noncopyable
{
 public:
 	//���캯��
  ThreadLocal()
  {
  //����һ��key,����destructorָ����key����ʱ�Ļص�����
    pthread_key_create(&pkey_, &ThreadLocal::destructor);
  }

//��������������key,ͬʱ�����destructor����keyָ���ֵ
  ~ThreadLocal()
  {
    pthread_key_delete(pkey_);
  }

//��ȡ�߳��ض�����
  T& value()
  {
 	//���ú���pthread_getspecific(key)��ȡkeyָ������� 
    T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));
//�������Ϊ��,�򴴽�������
	if (!perThreadValue) {
		
      T* newObj = new T();
      pthread_setspecific(pkey_, newObj);
      perThreadValue = newObj;
    }
    return *perThreadValue;
  }

 private:
//�������ݺ�����static��Ϊ�˷���pthread_key_create()�лص����������������ͣ�������������thisָ��
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
