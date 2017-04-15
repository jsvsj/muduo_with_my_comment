#ifndef MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
#define MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H

#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/Endian.h>
#include <muduo/net/TcpConnection.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

//��Ϣ�������,���TCP��ճ������
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

//��Ϣ����������Ļص�����
  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp receiveTime)
  {
  
  //ע���õ���while������if
//�����ж�����Ϣ��Ҫ��������ȥ��ͷ����������while
	while (buf->readableBytes() >= kHeaderLen) // kHeaderLen == 4
    {
      // FIXME: use Buffer::peekInt32()
	//�õ����ֽڵ�ָ��
	  const void* data = buf->peek();

	//��ð�ͷ,�����ֽ���  
      int32_t be32 = *static_cast<const int32_t*>(data); // SIGBUS

//ת���������ֽ���
	  const int32_t len = muduo::net::sockets::networkToHost32(be32);

	  if (len > 65536 || len < 0)
      {
        LOG_ERROR << "Invalid length " << len;

		conn->shutdown();  // FIXME: disable reading

		break;
      }
	  //����ﵽһ����������Ϣ�ĳ���
      else if (buf->readableBytes() >= len + kHeaderLen)
      {
      
      //������ͷ����
        buf->retrieve(kHeaderLen);

//ȡ����������
		muduo::string message(buf->peek(), len);

//�ص�ChatSercer����Ϣ�����Ļص��ص�����
		messageCallback_(conn, message, receiveTime);

//������������
		buf->retrieve(len);
      }
	  //�����Ϣ���Ȳ���
      else
      {
        break;
      }
    }
  }

  // FIXME: TcpConnectionPtr
  //������Ϣ
  void send(muduo::net::TcpConnection* conn,
            const muduo::StringPiece& message)
  {
    muduo::net::Buffer buf;

	//����Ϣ��ӵ���������
    buf.append(message.data(), message.size());

//��ȡ��Ϣ�ĳ���
	int32_t len = static_cast<int32_t>(message.size());
//ת���������ֽ��򣬼�����ֽ���
	int32_t be32 = muduo::net::sockets::hostToNetwork32(len);

//��Ӱ�ͷ
	buf.prepend(&be32, sizeof be32);

	//������Ϣ
	conn->send(&buf);
  }

 private:
 	
  StringMessageCallback messageCallback_;

  //��Ϣͷ���ĳ���
  const static size_t kHeaderLen = sizeof(int32_t);
};

#endif  // MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
