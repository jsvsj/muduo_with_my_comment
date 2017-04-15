#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

//缓冲区高水位回调函数
void onHighWaterMark(const TcpConnectionPtr& conn, size_t len)
{
  LOG_INFO << "HighWaterMark " << len;
}

const int kBufSize = 64*1024;
const char* g_file = NULL;

//连接状态的回调函数
void onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "FileServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");

  //如果是处于连接的状态
  if (conn->connected())
  {
    LOG_INFO << "FileServer - Sending file " << g_file
             << " to " << conn->peerAddress().toIpPort();
	
    conn->setHighWaterMarkCallback(onHighWaterMark, kBufSize+1);

//打开文件
    FILE* fp = ::fopen(g_file, "rb");
    if (fp)
    {
    //将TcpConnection对象与fp绑定
    //通过这种方法，我们就不需要额外再用一个map容器来管理对应关系
      conn->setContext(fp);

	  char buf[kBufSize];
	  //读取文件内容的一块
      size_t nread = ::fread(buf, 1, sizeof buf, fp);
		//发送文件
	  conn->send(buf, nread);
    }
    else
    {
      conn->shutdown();
      LOG_INFO << "FileServer - no such file";
    }
  }
  else
  {
    if (!conn->getContext().empty())
    {
      FILE* fp = boost::any_cast<FILE*>(conn->getContext());
      if (fp)
      {
        ::fclose(fp);
      }
    }
  }
}

//数据写完的回调函数
void onWriteComplete(const TcpConnectionPtr& conn)
{
//获取存储的文件指针
  FILE* fp = boost::any_cast<FILE*>(conn->getContext());
  char buf[kBufSize];

  size_t nread = ::fread(buf, 1, sizeof buf, fp);
  if (nread > 0)
  {
  //如果还没发送完，则再次发送
    conn->send(buf, nread);
  }
  else
  {
    ::fclose(fp);
    fp = NULL;
    conn->setContext(fp);
    conn->shutdown();
    LOG_INFO << "FileServer - done";
  }
}

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    g_file = argv[1];

    EventLoop loop;
    InetAddress listenAddr(2021);
    TcpServer server(&loop, listenAddr, "FileServer");
    server.setConnectionCallback(onConnection);
    server.setWriteCompleteCallback(onWriteComplete);
    server.start();
    loop.loop();
  }
  else
  {
    fprintf(stderr, "Usage: %s file_for_downloading\n", argv[0]);
  }
}

