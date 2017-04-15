#ifndef MUDUO_BASE_TIMESTAMP_H
#define MUDUO_BASE_TIMESTAMP_H

#include <muduo/base/copyable.h>
#include <muduo/base/Types.h>

#include <boost/operators.hpp>

namespace muduo
{

///
/// Time stamp in UTC, in microseconds resolution.
///
/// This class is immutable.
/// It's recommended to pass it by value, since it's passed in register on x64.
///
class Timestamp : public muduo::copyable,
                  public boost::less_than_comparable<Timestamp>

//boost::less_than_comparable<Timestamp>继承这个类，只要实现<,即可自动实现> <= >=函数
{
 public:
  ///
  /// Constucts an invalid Timestamp.
  ///
  //默认构造函数
  Timestamp()
    : microSecondsSinceEpoch_(0)
  {
  }

  ///
  /// Constucts a Timestamp at specific time
  ///
  /// @param microSecondsSinceEpoch
  explicit Timestamp(int64_t microSecondsSinceEpoch);

//交换两个时间戳对象中的微秒数
  void swap(Timestamp& that)
  {
    std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
  }

  // default copy/assignment/dtor are Okay

  string toString() const;
  string toFormattedString() const;

//是否是一个错误的时间
  bool valid() const { return microSecondsSinceEpoch_ > 0; }

  // for internal usage.
  int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
  time_t secondsSinceEpoch() const
  { return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond); }

  ///
  /// Get time of now.
  ///
  static Timestamp now();
  static Timestamp invalid();

//1秒=1000*1000微秒
  static const int kMicroSecondsPerSecond = 1000 * 1000;

 private:
 	//从1970年1月1日0点到现在流过的微秒数
  int64_t microSecondsSinceEpoch_;
};

//重载<操作符,由于继承了boost::less_than_comparable<Timestamp>,会可自动实现> <= >=函数
inline bool operator<(Timestamp lhs, Timestamp rhs)
{
  return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

//重载==操作符
inline bool operator==(Timestamp lhs, Timestamp rhs)
{
  return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

///
/// Gets time difference of two timestamps, result in seconds.
///
/// @param high, low
/// @return (high-low) in seconds
/// @c double has 52-bit precision, enough for one-microseciond
/// resolution for next 100 years.

//取两个时间戳的差值,结果的单位为秒
inline double timeDifference(Timestamp high, Timestamp low)
{
  int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
  return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}

///
/// Add @c seconds to given timestamp.
///
/// @return timestamp+seconds as Timestamp
///
//将当前保存的时间增加或减少seconds,单位为秒, 返回一个新的对象
inline Timestamp addTime(Timestamp timestamp, double seconds)
{
  int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
  return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

}
#endif  // MUDUO_BASE_TIMESTAMP_H
