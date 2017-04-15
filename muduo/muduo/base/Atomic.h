// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_ATOMIC_H
#define MUDUO_BASE_ATOMIC_H

#include <boost/noncopyable.hpp>
#include <stdint.h>

namespace muduo
{

namespace detail
{
template<typename T>


//ԭ�Ӳ�������

//ʹ��ԭ�Ӳ�������ʱҪ����-march=cpu-type
class AtomicIntegerT : boost::noncopyable
{

//boost::noncopyable,���ܿ�������,��=�뿽�����캯��˽��

 public:
  AtomicIntegerT()
    : value_(0)
  {
  }

  // uncomment if you need copying and assignment
  //
  // AtomicIntegerT(const AtomicIntegerT& that)
  //   : value_(that.get())
  // {}
  //
  // AtomicIntegerT& operator=(const AtomicIntegerT& that)
  // {
  //   getAndSet(that.get());
  //   return *this;
  // }


//��ȡ��ǰvalue��ֵ
  T get()
  {

  //���Ƶ�__sync_bool_compare_and_swap(&value_, 0, 0);
  //�ȱȽϣ������ã����Ƚϳɹ����򷵻�true
  
  //__sync_val_compare_and_swap(&value_, 0, 0)ԭ�ӽ�������,�̰߳�ȫ
  //����Ϊ  (type *ptr,type oldvalue, type newvalue)
  //���*ptr==oldvalue,�򷵻�odlvalue,����*ptr=newvalue
  //���򣬲�������*ptr=newvalue,��Ҳ�᷵��*ptr
    return __sync_val_compare_and_swap(&value_, 0, 0);
  }

//�Ȼ�ȡ����+
  T getAndAdd(T x)
  {
  //__sync_fetch_and_add(&value_, x)ԭ�����Ӻ������̰߳�ȫ
    return __sync_fetch_and_add(&value_, x);
  }

//��+�����ȡ
  T addAndGet(T x)
  {
    return getAndAdd(x) + x;
  }

//��ֵ+1�ٻ�ȡ
  T incrementAndGet()
  {
    return addAndGet(1);
  }

//-1�ٻ�ȡ
  T decrementAndGet()
  {
    return addAndGet(-1);
  }

//����
  void add(T x)
  {
    getAndAdd(x);
  }
//+1
  void increment()
  {
    incrementAndGet();
  }
//-1
  void decrement()
  {
    decrementAndGet();
  }

//�Ȼ�ȡ�ٸ�ֵ
  T getAndSet(T newValue)
  {
  
  //__sync_lock_test_and_set(&value_, newValue)ԭ�Ӹ�ֵ����

    return __sync_lock_test_and_set(&value_, newValue);
  }

//volatile������:
//��ֹ�������Դ�������Ż�,ϵͳ���Ǵ��ڴ��ж�ȡ�ñ�����ֵ
 private:
  volatile T value_;
};
}

//��ʵ����ģ��   32λ
typedef detail::AtomicIntegerT<int32_t> AtomicInt32;

//64λ
typedef detail::AtomicIntegerT<int64_t> AtomicInt64;
}

#endif  // MUDUO_BASE_ATOMIC_H
