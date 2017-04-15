// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_INSPECT_INSPECTOR_H
#define MUDUO_NET_INSPECT_INSPECTOR_H

#include <muduo/base/Mutex.h>
#include <muduo/net/http/HttpRequest.h>
#include <muduo/net/http/HttpServer.h>

#include <map>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{
namespace net
{

class ProcessInspector;

// A internal inspector of the running process, usually a singleton.
class Inspector : boost::noncopyable
{
 public:
  typedef std::vector<string> ArgList;
  typedef boost::function<string (HttpRequest::Method, const ArgList& args)> Callback;
  Inspector(EventLoop* loop,
            const InetAddress& httpAddr,
            const string& name);
  ~Inspector();

  void add(const string& module,
           const string& command,
           const Callback& cb,
           const string& help);

 private:
  typedef std::map<string, Callback> CommandList;

  typedef std::map<string, string> HelpList;

  void start();
  
  void onRequest(const HttpRequest& req, HttpResponse* resp);

  //Http服务器对象
  HttpServer server_;
  
  boost::scoped_ptr<ProcessInspector> processInspector_;
  MutexLock mutex_;

  //map<string,map<string, Callback>>,依次为模块，命令，回调函数
  std::map<string, CommandList> commands_;

//map<string,map<string,string>>,依次为模块，命令，帮助信息
  std::map<string, HelpList> helps_;
};

}
}

#endif  // MUDUO_NET_INSPECT_INSPECTOR_H
