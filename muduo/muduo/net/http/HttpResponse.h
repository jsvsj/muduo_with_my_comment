// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_HTTP_HTTPRESPONSE_H
#define MUDUO_NET_HTTP_HTTPRESPONSE_H

#include <muduo/base/copyable.h>
#include <muduo/base/Types.h>

#include <map>

namespace muduo
{
namespace net
{

class Buffer;
class HttpResponse : public muduo::copyable
{
 public:
 	//��Ӧ״̬��
  enum HttpStatusCode
  {
    kUnknown,
    k200Ok = 200,//����ɹ�
    k301MovedPermanently = 301,//�ض���
    k400BadRequest = 400,//���������,�﷨��ʽ�д��󣬷������޷����������
    k404NotFound = 404,//�������ҳ������
  };

  explicit HttpResponse(bool close)
    : statusCode_(kUnknown),
      closeConnection_(close)
  {
  }

  void setStatusCode(HttpStatusCode code)
  { statusCode_ = code; }

  void setStatusMessage(const string& message)
  { statusMessage_ = message; }

  void setCloseConnection(bool on)
  { closeConnection_ = on; }

  bool closeConnection() const
  { return closeConnection_; }

//�����ĵ�ý������
  void setContentType(const string& contentType)
  { addHeader("Content-Type", contentType); }

//���ͷ����Ϣ
  // FIXME: replace string with StringPiece
  void addHeader(const string& key, const string& value)
  { headers_[key] = value; }

  void setBody(const string& body)
  { body_ = body; }

  void appendToBuffer(Buffer* output) const;

 private:
 	//header�б�
  std::map<string, string> headers_;
//״̬��Ӧ��
  HttpStatusCode statusCode_;
  // FIXME: add http version
//״̬��Ӧ���Ӧ���ı���Ϣ
  string statusMessage_;

//�Ƿ�ر�����
  bool closeConnection_;

//ʵ��
  string body_;
};

}
}

#endif  // MUDUO_NET_HTTP_HTTPRESPONSE_H
