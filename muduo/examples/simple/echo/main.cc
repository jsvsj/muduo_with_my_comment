#include "echo.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

// using namespace muduo;
// using namespace muduo::net;

int main()
{
  LOG_INFO << "pid = " << getpid();

//loop�Ĺ��캯���л��ʼ��wakeupfd��wakeupchannel����ӵ�poll�м���
  
  muduo::net::EventLoop loop;
  muduo::net::InetAddress listenAddr(2007);
  EchoServer server(&loop, listenAddr);
  //�������߳��д���socet�׽��֣�����bind,listen,֮����ӵ�poll�м���������߳�������0���ᴴ���̳߳أ�
  server.start();
  loop.loop();
}

