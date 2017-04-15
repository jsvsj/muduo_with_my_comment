#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

const size_t frameLen = 2*sizeof(int64_t);


//服务端连接状态的回调函数
void serverConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->name() << " " << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
  //设置立即发送
    conn->setTcpNoDelay(true);
  }
  else
  {
  }
}


//设置服务端接收到消息的回调函数
void serverMessageCallback(const TcpConnectionPtr& conn,
                           Buffer* buffer,
                           muduo::Timestamp receiveTime)
{
  int64_t message[2];
  while (buffer->readableBytes() >= frameLen)
  {
    memcpy(message, buffer->peek(), frameLen);

	buffer->retrieve(frameLen);
	
	//保存接收的时刻
    message[1] = receiveTime.microSecondsSinceEpoch();

	conn->send(message, sizeof message);
  }
}

//以服务端运行
void runServer(uint16_t port)
{
  EventLoop loop;
  TcpServer server(&loop, InetAddress(port), "ClockServer");
  
  server.setConnectionCallback(serverConnectionCallback);

  server.setMessageCallback(serverMessageCallback);

  server.start();

  loop.loop();
}

TcpConnectionPtr clientConnection;


//客户端连接的回调函数
void clientConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
  //全局变量
    clientConnection = conn;

//设置立即发送
	conn->setTcpNoDelay(true);
  }
  else
  {
    clientConnection.reset();
  }
}

//设置客户端接收到消息的回调函数
void clientMessageCallback(const TcpConnectionPtr&,
                           Buffer* buffer,
                           muduo::Timestamp receiveTime)
{
  int64_t message[2];
  while (buffer->readableBytes() >= frameLen)
  {
    memcpy(message, buffer->peek(), frameLen);
    buffer->retrieve(frameLen);

//发送的时刻
	int64_t send = message[0];

//服务端接收的时刻
	int64_t their = message[1];

//客户端接收的时刻
	int64_t back = receiveTime.microSecondsSinceEpoch();

	int64_t mine = (back+send)/2;

	LOG_INFO << "round trip " << back - send
             << " clock error " << their - mine;
  }
}

//客户端执行
void sendMyTime()
{
  if (clientConnection)
  {
    int64_t message[2] = { 0, 0 };
	//保存发送的时刻
    message[0] = Timestamp::now().microSecondsSinceEpoch();

	clientConnection->send(message, sizeof message);
  }
}

//以客户端运行
void runClient(const char* ip, uint16_t port)
{
  EventLoop loop;
  TcpClient client(&loop, InetAddress(ip, port), "ClockClient");
  
  client.enableRetry();

//设置连接的回调函数
  client.setConnectionCallback(clientConnectionCallback);

//设置消息到来的回调函数
  client.setMessageCallback(clientMessageCallback);

//发起连接
  client.connect();

//每隔02秒执行sendMyTime函数
  loop.runEvery(0.2, sendMyTime);

  loop.loop();
}

int main(int argc, char* argv[])
{
  if (argc > 2)
  {
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
	//如果argv[1]为"-s",则以服务端运行
    if (strcmp(argv[1], "-s") == 0)
    {
      runServer(port);
    }
	//以客户端运行
    else
    {
      runClient(argv[1], port);
    }
  }
  else
  {
    printf("Usage:\n%s -s port\n%s ip port\n", argv[0], argv[0]);
  }
}

