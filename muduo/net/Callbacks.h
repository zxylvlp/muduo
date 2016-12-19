// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_CALLBACKS_H
#define MUDUO_NET_CALLBACKS_H

#include <muduo/base/Timestamp.h>

#include <functional>
#include <memory>

namespace muduo
{

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

// should really belong to base/Types.h, but <memory> is not included there.
/**
 * 获取智能指针包裹的指针
 */
template<typename T>
inline T* get_pointer(const std::shared_ptr<T>& ptr)
{
  return ptr.get();
}

/**
 * 获取唯一指针包裹的指针
 */
template<typename T>
inline T* get_pointer(const std::unique_ptr<T>& ptr)
{
  return ptr.get();
}

// Adapted from google-protobuf stubs/common.h
// see License in muduo/base/Types.h
/**
 * 指针向下转型
 */
template<typename To, typename From>
inline ::std::shared_ptr<To> down_pointer_cast(const ::std::shared_ptr<From>& f) {
  if (false)
  {
    implicit_cast<From*, To*>(0);
  }

#ifndef NDEBUG
  assert(f == NULL || dynamic_cast<To*>(get_pointer(f)) != NULL);
#endif
  return ::std::static_pointer_cast<To>(f);
}

namespace net
{

// All client visible callbacks go here.

class Buffer;
class TcpConnection;
/**
 * 定义tcp连接类的智能指针类型
 */
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
/**
 * 定义定时器回调函数类型
 */
typedef std::function<void()> TimerCallback;
/**
 * 定义连接回调函数类型
 */
typedef std::function<void (const TcpConnectionPtr&)> ConnectionCallback;
/**
 * 定义关闭回调函数类型
 */
typedef std::function<void (const TcpConnectionPtr&)> CloseCallback;
/**
 * 定义写完毕回调函数类型
 */
typedef std::function<void (const TcpConnectionPtr&)> WriteCompleteCallback;
/**
 * 定义高水位回调函数类型
 */
typedef std::function<void (const TcpConnectionPtr&, size_t)> HighWaterMarkCallback;

// the data has been read to (buf, len)
/**
 * 定义消息回调函数类型
 */
typedef std::function<void (const TcpConnectionPtr&,
                            Buffer*,
                            Timestamp)> MessageCallback;

void defaultConnectionCallback(const TcpConnectionPtr& conn);
void defaultMessageCallback(const TcpConnectionPtr& conn,
                            Buffer* buffer,
                            Timestamp receiveTime);

}
}

#endif  // MUDUO_NET_CALLBACKS_H
