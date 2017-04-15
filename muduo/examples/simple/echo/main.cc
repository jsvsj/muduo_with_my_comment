#include "echo.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

// using namespace muduo;
// using namespace muduo::net;

int main()
{
  LOG_INFO << "pid = " << getpid();

//loop的构造函数中会初始化wakeupfd，wakeupchannel，添加到poll中监听
  
  muduo::net::EventLoop loop;
  muduo::net::InetAddress listenAddr(2007);
  EchoServer server(&loop, listenAddr);
  //会在主线程中创建socet套接字，调用bind,listen,之后添加到poll中监听，如果线程数大于0，会创建线程池，
  server.start();
  loop.loop();
}

