#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

const size_t frameLen = 2*sizeof(int64_t);


//���������״̬�Ļص�����
void serverConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->name() << " " << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
  //������������
    conn->setTcpNoDelay(true);
  }
  else
  {
  }
}


//���÷���˽��յ���Ϣ�Ļص�����
void serverMessageCallback(const TcpConnectionPtr& conn,
                           Buffer* buffer,
                           muduo::Timestamp receiveTime)
{
  int64_t message[2];
  while (buffer->readableBytes() >= frameLen)
  {
    memcpy(message, buffer->peek(), frameLen);

	buffer->retrieve(frameLen);
	
	//������յ�ʱ��
    message[1] = receiveTime.microSecondsSinceEpoch();

	conn->send(message, sizeof message);
  }
}

//�Է��������
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


//�ͻ������ӵĻص�����
void clientConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
  //ȫ�ֱ���
    clientConnection = conn;

//������������
	conn->setTcpNoDelay(true);
  }
  else
  {
    clientConnection.reset();
  }
}

//���ÿͻ��˽��յ���Ϣ�Ļص�����
void clientMessageCallback(const TcpConnectionPtr&,
                           Buffer* buffer,
                           muduo::Timestamp receiveTime)
{
  int64_t message[2];
  while (buffer->readableBytes() >= frameLen)
  {
    memcpy(message, buffer->peek(), frameLen);
    buffer->retrieve(frameLen);

//���͵�ʱ��
	int64_t send = message[0];

//����˽��յ�ʱ��
	int64_t their = message[1];

//�ͻ��˽��յ�ʱ��
	int64_t back = receiveTime.microSecondsSinceEpoch();

	int64_t mine = (back+send)/2;

	LOG_INFO << "round trip " << back - send
             << " clock error " << their - mine;
  }
}

//�ͻ���ִ��
void sendMyTime()
{
  if (clientConnection)
  {
    int64_t message[2] = { 0, 0 };
	//���淢�͵�ʱ��
    message[0] = Timestamp::now().microSecondsSinceEpoch();

	clientConnection->send(message, sizeof message);
  }
}

//�Կͻ�������
void runClient(const char* ip, uint16_t port)
{
  EventLoop loop;
  TcpClient client(&loop, InetAddress(ip, port), "ClockClient");
  
  client.enableRetry();

//�������ӵĻص�����
  client.setConnectionCallback(clientConnectionCallback);

//������Ϣ�����Ļص�����
  client.setMessageCallback(clientMessageCallback);

//��������
  client.connect();

//ÿ��02��ִ��sendMyTime����
  loop.runEvery(0.2, sendMyTime);

  loop.loop();
}

int main(int argc, char* argv[])
{
  if (argc > 2)
  {
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
	//���argv[1]Ϊ"-s",���Է��������
    if (strcmp(argv[1], "-s") == 0)
    {
      runServer(port);
    }
	//�Կͻ�������
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

