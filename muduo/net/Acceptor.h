// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <functional>

#include <muduo/net/Channel.h>
#include <muduo/net/Socket.h>

namespace muduo
{
namespace net
{

class EventLoop;
class InetAddress;

///
/// Acceptor of incoming TCP connections.
///
class Acceptor : noncopyable
{
 public:
  /**
   * 定义新建连接回调函数类型
   */
  typedef std::function<void (int sockfd, const InetAddress&)> NewConnectionCallback;

  Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
  ~Acceptor();

  /**
   * 设置新建连接回调函数
   */
  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }

  /**
   * 返回是否正在监听
   */
  bool listenning() const { return listenning_; }
  void listen();

 private:
  void handleRead();

  /**
   * 指向事件循环的指针
   */
  EventLoop* loop_;
  /**
   * 接受套接字
   */
  Socket acceptSocket_;
  /**
   * 接受通道
   */
  Channel acceptChannel_;
  /**
   * 新建连接回调
   */
  NewConnectionCallback newConnectionCallback_;
  /**
   * 是否正在监听
   */
  bool listenning_;
  /**
   * 闲置描述符
   */
  int idleFd_;
};

}
}

#endif  // MUDUO_NET_ACCEPTOR_H
