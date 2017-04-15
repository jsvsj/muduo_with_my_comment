// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

#include <muduo/net/http/HttpServer.h>

#include <muduo/base/Logging.h>
#include <muduo/net/http/HttpContext.h>
#include <muduo/net/http/HttpRequest.h>
#include <muduo/net/http/HttpResponse.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

namespace muduo
{
namespace net
{
namespace detail
{

// FIXME: move to HttpContext class
//解析请求行
bool processRequestLine(const char* begin, const char* end, HttpContext* context)
{
  bool succeed = false;
  const char* start = begin;
  const char* space = std::find(start, end, ' ');
  HttpRequest& request = context->request();
  if (space != end && request.setMethod(start, space))
  {
    start = space+1;
    space = std::find(start, end, ' ');
    if (space != end)
    {
      request.setPath(start, space);//解析路径
      start = space+1;
      succeed = end-start == 8 && std::equal(start, end-1, "HTTP/1.");
      if (succeed)
      {
        if (*(end-1) == '1')
        {
          request.setVersion(HttpRequest::kHttp11);//Http1.1
        }
        else if (*(end-1) == '0')
        {
          request.setVersion(HttpRequest::kHttp10);//Http1.0
        }
        else
        {
          succeed = false;
        }
      }
    }
  }
  return succeed;
}

// FIXME: move to HttpContext class
// return false if any error
//解析请求
bool parseRequest(Buffer* buf, HttpContext* context, Timestamp receiveTime)
{
  bool ok = true;
  bool hasMore = true;
  while (hasMore)
  {
  //如果处于解析请求行状态
    if (context->expectRequestLine())
    {
    //查找\r\n
      const char* crlf = buf->findCRLF();
      if (crlf)
      {
      //解析请求行
        ok = processRequestLine(buf->peek(), crlf, context);
        if (ok)
        {
        //设置请求时间
          context->request().setReceiveTime(receiveTime);
		//将请求行从buf中取回，包括\r\n
          buf->retrieveUntil(crlf + 2);

		//将HttpContext状态更改为kExpectHeaders
          context->receiveRequestLine();
        }
        else
        {
          hasMore = false;
        }
      }
      else
      {
        hasMore = false;
      }
    }
	
	//处于解析请求头状态
    else if (context->expectHeaders())//解析header
    {
      const char* crlf = buf->findCRLF();
      if (crlf)
      {
        const char* colon = std::find(buf->peek(), crlf, ':');//冒号的位置
        if (colon != crlf)
        {
          context->request().addHeader(buf->peek(), colon, crlf);
        }
        else
        {
          // empty line, end of header
          context->receiveHeaders();//Httpcontext的状态改为kCotAll
		  
          hasMore = !context->gotAll();
        }
		
        buf->retrieveUntil(crlf + 2);//将header从buf中取回，包括\r\n
      }
      else
      {
        hasMore = false;
      }
    }

	//处于解析请求体状态,当前还不支持
    else if (context->expectBody())
    {
      // FIXME:
    }
  }
  return ok;
}

void defaultHttpCallback(const HttpRequest&, HttpResponse* resp)
{
  resp->setStatusCode(HttpResponse::k404NotFound);
  resp->setStatusMessage("Not Found");
  resp->setCloseConnection(true);
}

}
}
}

HttpServer::HttpServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       const string& name)
  : server_(loop, listenAddr, name),
    httpCallback_(detail::defaultHttpCallback)
{
  server_.setConnectionCallback(
      boost::bind(&HttpServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&HttpServer::onMessage, this, _1, _2, _3));
}

HttpServer::~HttpServer()
{
}

void HttpServer::start()
{
  LOG_WARN << "HttpServer[" << server_.name()
    << "] starts listenning on " << server_.hostport();
  server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    conn->setContext(HttpContext());
  }
}

//消息到来时的回调函数,会调用OnRequest()函数
void HttpServer::onMessage(const TcpConnectionPtr& conn,
                           Buffer* buf,
                           Timestamp receiveTime)
{
  HttpContext* context = boost::any_cast<HttpContext>(conn->getMutableContext());

//解析请求
  if (!detail::parseRequest(buf, context, receiveTime))
  {
  //如果请求失败
    conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
    conn->shutdown();
  }

//请求消息解析完毕
  if (context->gotAll())
  {
    onRequest(conn, context->request());
	//重置context,适用于长连接
    context->reset();
  }
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
  const string& connection = req.getHeader("Connection");
  bool close = connection == "close" ||
    (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
  HttpResponse response(close);

  //回调用户设定的函数
  httpCallback_(req, &response);
  Buffer buf;
  response.appendToBuffer(&buf);
  conn->send(&buf);
  if (response.closeConnection())
  {
    conn->shutdown();
  }
}

