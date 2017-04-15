#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/shared_ptr.hpp>

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

//智能指针，管理文件指针
typedef boost::shared_ptr<FILE> FilePtr;

//连接状态的回调函数
void onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "FileServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");
  //如果连接了
  if (conn->connected())
  {
    LOG_INFO << "FileServer - Sending file " << g_file
             << " to " << conn->peerAddress().toIpPort();
    conn->setHighWaterMarkCallback(onHighWaterMark, kBufSize+1);

//打开文件
    FILE* fp = ::fopen(g_file, "rb");
    if (fp)
    {
    //初始化智能指针
      FilePtr ctx(fp, ::fclose);
//两个参数，表示ctx引用计数减为0时，调用fclose()销毁fp

      conn->setContext(ctx);
      char buf[kBufSize];
      size_t nread = ::fread(buf, 1, sizeof buf, fp);
      conn->send(buf, nread);
    }
    else
    {
      conn->shutdown();
      LOG_INFO << "FileServer - no such file";
    }
  }
}

void onWriteComplete(const TcpConnectionPtr& conn)
{
  const FilePtr& fp = boost::any_cast<const FilePtr&>(conn->getContext());
  char buf[kBufSize];
  size_t nread = ::fread(buf, 1, sizeof buf, get_pointer(fp));
  if (nread > 0)
  {
    conn->send(buf, nread);
  }
  else
  {
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

