// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Exception.h>

//#include <cxxabi.h>
#include <execinfo.h>
#include <stdlib.h>

using namespace muduo;

Exception::Exception(const char* msg)
  : message_(msg)
{
  fillStackTrace();
}

//���캯��,�ִ��
Exception::Exception(const string& msg)
  : message_(msg)
{
  fillStackTrace();
}

Exception::~Exception() throw ()
{

}

//message��string����
const char* Exception::what() const throw()
{
  return message_.c_str();
}

//����stackָ��,stack��string����
const char* Exception::stackTrace() const throw()
{
  return stack_.c_str();
}

//���캯���е��� 
void Exception::fillStackTrace()
{
  const int len = 200;
  
  //ָ�����飬�����ַ
  void* buffer[len];

  //backtrace(...)������ջ���ݣ��������ջ֡�ĵ�ַ
  int nptrs = ::backtrace(buffer, len);
  
  //���ݵ�ַ��ת������Ӧ�ĺ�������,����ָ������,�ڲ����õ�malloc����Ҫ�ֶ��ͷ��ڴ�
  char** strings = ::backtrace_symbols(buffer, nptrs);

  if (strings)
  {
    for (int i = 0; i < nptrs; ++i)
    {
      // TODO demangle funcion name with abi::__cxa_demangle
      //��ջ�к�����Ϣ��ӵ�stack_��
      stack_.append(strings[i]);
      stack_.push_back('\n');
    }
	//�ֶ��ͷ��ڴ�,ָ������,����Ҫ�ͷ������е�ָ��ָ����ڴ�
    free(strings);
  }
}

