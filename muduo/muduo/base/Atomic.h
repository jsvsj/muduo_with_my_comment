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


//原子操作的类

//使用原子操作编译时要加上-march=cpu-type
class AtomicIntegerT : boost::noncopyable
{

//boost::noncopyable,不能拷贝的类,将=与拷贝构造函数私有

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


//获取当前value的值
  T get()
  {

  //类似的__sync_bool_compare_and_swap(&value_, 0, 0);
  //先比较，再设置，若比较成功，则返回true
  
  //__sync_val_compare_and_swap(&value_, 0, 0)原子交换函数,线程安全
  //参数为  (type *ptr,type oldvalue, type newvalue)
  //如果*ptr==oldvalue,则返回odlvalue,并将*ptr=newvalue
  //否则，不会设置*ptr=newvalue,但也会返回*ptr
    return __sync_val_compare_and_swap(&value_, 0, 0);
  }

//先获取，在+
  T getAndAdd(T x)
  {
  //__sync_fetch_and_add(&value_, x)原子增加函数，线程安全
    return __sync_fetch_and_add(&value_, x);
  }

//先+，后获取
  T addAndGet(T x)
  {
    return getAndAdd(x) + x;
  }

//将值+1再获取
  T incrementAndGet()
  {
    return addAndGet(1);
  }

//-1再获取
  T decrementAndGet()
  {
    return addAndGet(-1);
  }

//增加
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

//先获取再赋值
  T getAndSet(T newValue)
  {
  
  //__sync_lock_test_and_set(&value_, newValue)原子赋值操作

    return __sync_lock_test_and_set(&value_, newValue);
  }

//volatile的做用:
//防止编译器对代码进行优化,系统总是从内存中读取该变量的值
 private:
  volatile T value_;
};
}

//对实例化模板   32位
typedef detail::AtomicIntegerT<int32_t> AtomicInt32;

//64位
typedef detail::AtomicIntegerT<int64_t> AtomicInt64;
}

#endif  // MUDUO_BASE_ATOMIC_H
