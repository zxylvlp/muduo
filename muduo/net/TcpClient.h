// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCLIENT_H
#define MUDUO_NET_TCPCLIENT_H

#include <muduo/base/Mutex.h>
#include <muduo/net/TcpConnection.h>

namespace muduo
{
namespace net
{

class Connector;
/**
 * 定义连接器的智能指针类型
 */
typedef std::shared_ptr<Connector> ConnectorPtr;

/**
 * tcp客户端类
 */
class TcpClient : noncopyable
{
 public:
  // TcpClient(EventLoop* loop);
  // TcpClient(EventLoop* loop, const string& host, uint16_t port);
  TcpClient(EventLoop* loop,
            const InetAddress& serverAddr,
            const string& nameArg);
  ~TcpClient();  // force out-line dtor, for std::unique_ptr members.

  void connect();
  void disconnect();
  void stop();

  /**
   * 获取连接
   *
   * 在互斥锁保护下返回tcp连接的指针
   */
  TcpConnectionPtr connection() const
  {
    MutexLockGuard lock(mutex_);
    return connection_;
  }

  /**
   * 返回指向事件循环的指针
   */
  EventLoop* getLoop() const { return loop_; }
  /**
   * 返回是否可以重试
   */
  bool retry() const { return retry_; }
  /**
   * 允许重试
   */
  void enableRetry() { retry_ = true; }

  /**
   * 返回名字
   */
  const string& name() const
  { return name_; }

  /// Set connection callback.
  /// Not thread safe.
  /**
   * 设置连接回调函数
   */
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  /// Set message callback.
  /// Not thread safe.
  /**
   * 设置有消息回调函数
   */
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  /// Set write complete callback.
  /// Not thread safe.
  /**
   * 设置写完成回调函数
   */
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void setConnectionCallback(ConnectionCallback&& cb)
  { connectionCallback_ = std::move(cb); }
  void setMessageCallback(MessageCallback&& cb)
  { messageCallback_ = std::move(cb); }
  void setWriteCompleteCallback(WriteCompleteCallback&& cb)
  { writeCompleteCallback_ = std::move(cb); }
#endif

 private:
  /// Not thread safe, but in loop
  void newConnection(int sockfd);
  /// Not thread safe, but in loop
  void removeConnection(const TcpConnectionPtr& conn);

  /**
   * 指向事件循环的指针
   */
  EventLoop* loop_;
  /**
   * 指向连接器的智能指针
   */
  ConnectorPtr connector_; // avoid revealing Connector
  /**
   * 名字
   */
  const string name_;
  /**
   * 连接回调函数
   */
  ConnectionCallback connectionCallback_;
  /**
   * 有消息回调函数
   */
  MessageCallback messageCallback_;
  /**
   * 写完成回调函数
   */
  WriteCompleteCallback writeCompleteCallback_;
  /**
   * 是否重试
   */
  bool retry_;   // atomic
  /**
   * 是否连接
   */
  bool connect_; // atomic
  // always in loop thread
  /**
   * 下一个连接id
   */
  int nextConnId_;
  /**
   * 互斥锁对象
   */
  mutable MutexLock mutex_;
  /**
   * 指向tcp连接的智能指针
   */
  TcpConnectionPtr connection_; // @GuardedBy mutex_
};

}
}

#endif  // MUDUO_NET_TCPCLIENT_H
