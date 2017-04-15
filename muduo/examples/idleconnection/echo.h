#ifndef MUDUO_EXAMPLES_IDLECONNECTION_ECHO_H
#define MUDUO_EXAMPLES_IDLECONNECTION_ECHO_H

#include <muduo/net/TcpServer.h>
//#include <muduo/base/Types.h>

#include <boost/circular_buffer.hpp>
#include <boost/unordered_set.hpp>
#include <boost/version.hpp>

#if BOOST_VERSION < 104700
namespace boost
{
template <typename T>
inline size_t hash_value(const boost::shared_ptr<T>& x)
{
  return boost::hash_value(x.get());
}
}
#endif

// RFC 862
class EchoServer
{
 public:
  EchoServer(muduo::net::EventLoop* loop,
             const muduo::net::InetAddress& listenAddr,
             int idleSeconds);

  void start();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp time);

  void onTimer();

  void dumpConnectionBuckets() const;

//弱引用的智能指针
  typedef boost::weak_ptr<muduo::net::TcpConnection> WeakTcpConnectionPtr;

//结构体
  struct Entry : public muduo::copyable
  {
    explicit Entry(const WeakTcpConnectionPtr& weakConn)
      : weakConn_(weakConn)
    {
    }

    ~Entry()
    {

    //在muduo::net中有
    //typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;
        //获得管理对象的share_ptr
      muduo::net::TcpConnectionPtr conn = weakConn_.lock();

	  if (conn)
      {
        conn->shutdown();
      }
    }

    WeakTcpConnectionPtr weakConn_;
  };

  //unorder_set中的每个元素是EntryPtr,智能指针
  typedef boost::shared_ptr<Entry> EntryPtr;

  //弱引用的智能指针
  typedef boost::weak_ptr<Entry> WeakEntryPtr;

  //环形缓冲区每个格子存放的是一个unordered_set
  //std::set会自动排序，而unorderd_set不会，这样效率会更高
  typedef boost::unordered_set<EntryPtr> Bucket;

  //环形缓冲区
  typedef boost::circular_buffer<Bucket> WeakConnectionList;

  muduo::net::EventLoop* loop_;
  muduo::net::TcpServer server_;
  WeakConnectionList connectionBuckets_;
};

#endif  // MUDUO_EXAMPLES_IDLECONNECTION_ECHO_H
