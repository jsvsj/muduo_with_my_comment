#ifndef MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
#define MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H

#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/Endian.h>
#include <muduo/net/TcpConnection.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

//消息编解码类,解决TCP的粘包问题
class LengthHeaderCodec : boost::noncopyable
{
 public:
  typedef boost::function<void (const muduo::net::TcpConnectionPtr&,
                                const muduo::string& message,
                                muduo::Timestamp)> StringMessageCallback;

  explicit LengthHeaderCodec(const StringMessageCallback& cb)
    : messageCallback_(cb)
  {
  }

//消息到达服务器的回调函数
  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp receiveTime)
  {
  
  //注意用的是while而不是if
//可能有多条消息需要解析，即去除头部，所有用while
	while (buf->readableBytes() >= kHeaderLen) // kHeaderLen == 4
    {
      // FIXME: use Buffer::peekInt32()
	//得到首字节的指针
	  const void* data = buf->peek();

	//获得包头,网络字节序  
      int32_t be32 = *static_cast<const int32_t*>(data); // SIGBUS

//转换成主机字节序
	  const int32_t len = muduo::net::sockets::networkToHost32(be32);

	  if (len > 65536 || len < 0)
      {
        LOG_ERROR << "Invalid length " << len;

		conn->shutdown();  // FIXME: disable reading

		break;
      }
	  //如果达到一条完整的消息的长度
      else if (buf->readableBytes() >= len + kHeaderLen)
      {
      
      //丢弃包头数据
        buf->retrieve(kHeaderLen);

//取出包体数据
		muduo::string message(buf->peek(), len);

//回调ChatSercer中消息到来的回调回调函数
		messageCallback_(conn, message, receiveTime);

//丢弃包体数据
		buf->retrieve(len);
      }
	  //如果消息长度不够
      else
      {
        break;
      }
    }
  }

  // FIXME: TcpConnectionPtr
  //发送消息
  void send(muduo::net::TcpConnection* conn,
            const muduo::StringPiece& message)
  {
    muduo::net::Buffer buf;

	//将消息添加到缓冲区中
    buf.append(message.data(), message.size());

//获取消息的长度
	int32_t len = static_cast<int32_t>(message.size());
//转换成网络字节序，即大端字节序
	int32_t be32 = muduo::net::sockets::hostToNetwork32(len);

//添加包头
	buf.prepend(&be32, sizeof be32);

	//发送消息
	conn->send(&buf);
  }

 private:
 	
  StringMessageCallback messageCallback_;

  //消息头部的长度
  const static size_t kHeaderLen = sizeof(int32_t);
};

#endif  // MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
