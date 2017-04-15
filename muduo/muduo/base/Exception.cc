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

//¹¹Ôìº¯Êı,ÖÖ´ĞĞ
Exception::Exception(const string& msg)
  : message_(msg)
{
  fillStackTrace();
}

Exception::~Exception() throw ()
{

}

//messageÊÇstringÀàĞÍ
const char* Exception::what() const throw()
{
  return message_.c_str();
}

//·µ»ØstackÖ¸Õë,stackÊÇstringÀàĞÍ
const char* Exception::stackTrace() const throw()
{
  return stack_.c_str();
}

//¹¹Ôìº¯ÊıÖĞµ÷ÓÃ 
void Exception::fillStackTrace()
{
  const int len = 200;
  
  //Ö¸ÕëÊı×é£¬±£´æµØÖ·
  void* buffer[len];

  //backtrace(...)º¯Êı£¬Õ»»ØËİ£¬±£´æ¸÷¸öÕ»Ö¡µÄµØÖ·
  int nptrs = ::backtrace(buffer, len);
  
  //¸ù¾İµØÖ·£¬×ª»»³ÉÏàÓ¦µÄº¯Êı·ûºÅ,·µ»ØÖ¸ÕëÊı×é,ÄÚ²¿µ÷ÓÃµÄmalloc£¬ĞèÒªÊÖ¶¯ÊÍ·ÅÄÚ´æ
  char** strings = ::backtrace_symbols(buffer, nptrs);

  if (strings)
  {
    for (int i = 0; i < nptrs; ++i)
    {
      // TODO demangle funcion name with abi::__cxa_demangle
      //½«Õ»ÖĞº¯ÊıĞÅÏ¢Ìí¼Óµ½stack_ÖĞ
      stack_.append(strings[i]);
      stack_.push_back('\n');
    }
	//ÊÖ¶¯ÊÍ·ÅÄÚ´æ,Ö¸ÕëÊı×é,²»ĞèÒªÊÍ·ÅÊı×éÖĞµÄÖ¸ÕëÖ¸ÏòµÄÄÚ´æ
    free(strings);
  }
}

