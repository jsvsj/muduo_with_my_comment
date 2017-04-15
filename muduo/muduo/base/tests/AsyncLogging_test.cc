#include <muduo/base/AsyncLogging.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Timestamp.h>

#include <stdio.h>
#include <sys/resource.h>


//��־�ļ�������С
int kRollSize = 500*1000*1000;


muduo::AsyncLogging* g_asyncLog = NULL;


//��־���ʱ�Ļص�����
void asyncOutput(const char* msg, int len)
{
  g_asyncLog->append(msg, len);
}

void bench(bool longLog)
{

//������־������ǵĻص�����
  muduo::Logger::setOutput(asyncOutput);

  int cnt = 0;
  const int kBatch = 1000;
  muduo::string empty = " ";
  muduo::string longStr(3000, 'X');
  longStr += " ";

  for (int t = 0; t < 30; ++t)
  {
    muduo::Timestamp start = muduo::Timestamp::now();
    for (int i = 0; i < kBatch; ++i)
    {
      LOG_INFO << "Hello 0123456789" << " abcdefghijklmnopqrstuvwxyz "
	  			//����ǳ�����־����д��logstr,����д��" " 
               << (longLog ? longStr : empty)
               //����
               << cnt;
      ++cnt;
    }
    muduo::Timestamp end = muduo::Timestamp::now();

	//��ʱ���ٺ���
    printf("%f\n", timeDifference(end, start)*1000000/kBatch);

	struct timespec ts = { 0, 500*1000*1000 };

	nanosleep(&ts, NULL);
  }
}

int main(int argc, char* argv[])
{
  {
    // set max virtual memory to 2GB.
    size_t kOneGB = 1000*1024*1024;
    rlimit rl = { 2*kOneGB, 2*kOneGB };

	//���ý��������ڴ治�ܳ���2G
    setrlimit(RLIMIT_AS, &rl);
  }

  printf("pid = %d\n", getpid());

  char name[256];
  strncpy(name, argv[0], 256);

  //�첽��־����
  muduo::AsyncLogging log(::basename(name), kRollSize);
  log.start();
  
  g_asyncLog = &log;

//���argc>1,��Ϊ������־
  bool longLog = argc > 1;
  
  bench(longLog);
}
