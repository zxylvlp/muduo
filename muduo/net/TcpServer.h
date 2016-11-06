// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPSERVER_H
#define MUDUO_NET_TCPSERVER_H

#include <muduo/base/Atomic.h>
#include <muduo/base/Types.h>
#include <muduo/net/TcpConnection.h>

#include <map>

namespace muduo
{
namespace net
{

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

///
/// TCP server, supports single-threaded and thread-pool models.
///
/// This is an interface class, so don't expose too much details.
/**
 * TCP服务器类
 */
class TcpServer : noncopyable
{
 public:
  /**
   * 线程初始化回调函数类型，此函数返回值为void，参数为EventLoop指针
   */
  typedef std::function<void(EventLoop*)> ThreadInitCallback;
  /**
   * 指定是否重用处于time_wait的端口的选项
   */
  enum Option
  {
    kNoReusePort,
    kReusePort,
  };

  //TcpServer(EventLoop* loop, const InetAddress& listenAddr);
  TcpServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg,
            Option option = kNoReusePort);
  ~TcpServer();  // force out-line dtor, for std::unique_ptr members.

  /**
   * 返回ip地址和端口号
   */
  const string& ipPort() const { return ipPort_; }
  /**
   * 返回服务器的名字
   */
  const string& name() const { return name_; }
  /**
   * 返回指向EventLoop的指针
   */
  EventLoop* getLoop() const { return loop_; }

  /// Set the number of threads for handling input.
  ///
  /// Always accepts new connection in loop's thread.
  /// Must be called before @c start
  /// @param numThreads
  /// - 0 means all I/O in loop's thread, no thread will created.
  ///   this is the default value.
  /// - 1 means all I/O in another thread.
  /// - N means a thread pool with N threads, new connections
  ///   are assigned on a round-robin basis.
  void setThreadNum(int numThreads);
  /**
   * 设置线程初始化回电函数
   */
  void setThreadInitCallback(const ThreadInitCallback& cb)
  { threadInitCallback_ = cb; }
  /// valid after calling start()
  /**
   * 返回指向EventLoop线程池的智能指针
   */
  std::shared_ptr<EventLoopThreadPool> threadPool()
  { return threadPool_; }

  /// Starts the server if it's not listenning.
  ///
  /// It's harmless to call it multiple times.
  /// Thread safe.
  void start();

  /// Set connection callback.
  /// Not thread safe.
  /**
   * 连接成功回调函数
   */
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  /// Set message callback.
  /// Not thread safe.
  /**
   * 设置收到消息回调函数
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

 private:
  /// Not thread safe, but in loop
  void newConnection(int sockfd, const InetAddress& peerAddr);
  /// Thread safe.
  void removeConnection(const TcpConnectionPtr& conn);
  /// Not thread safe, but in loop
  void removeConnectionInLoop(const TcpConnectionPtr& conn);

  /**
   * 定义ConnectionMap类型，他是从字符串到指向TcpConnection智能指针的映射
   */
  typedef std::map<string, TcpConnectionPtr> ConnectionMap;

  /**
   * 指向EventLoop的指针
   */
  EventLoop* loop_;  // the acceptor loop
  /**
   * 服务器的ip地址加端口号
   */
  const string ipPort_;
  /**
   * 服务器的名称
   */
  const string name_;
  /**
   * 指向Acceptor的唯一指针
   */
  std::unique_ptr<Acceptor> acceptor_; // avoid revealing Acceptor
  /**
   * 指向EventLoop线程池的智能指针
   */
  std::shared_ptr<EventLoopThreadPool> threadPool_;
  /**
   * 连接成功回调函数
   */
  ConnectionCallback connectionCallback_;
  /**
   * 收到消息回调函数
   */
  MessageCallback messageCallback_;
  /**
   * 写完成回调函数
   */
  WriteCompleteCallback writeCompleteCallback_;
  /**
   * 线程初始化回调函数
   */
  ThreadInitCallback threadInitCallback_;
  /**
   * 服务器是否已经启动
   */
  AtomicInt32 started_;
  // always in loop thread
  /**
   * 下一个连接的id
   */
  int nextConnId_;
  /**
   * 存放连接的map
   */
  ConnectionMap connections_;
};

}
}

#endif  // MUDUO_NET_TCPSERVER_H
