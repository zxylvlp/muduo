// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CONNECTOR_H
#define MUDUO_NET_CONNECTOR_H

#include <muduo/net/InetAddress.h>

#include <functional>
#include <memory>

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;

/**
 * 连接者类
 */
class Connector : noncopyable,
                  public std::enable_shared_from_this<Connector>
{
 public:
  /**
   * 定义新建连接回调函数类型
   */
  typedef std::function<void (int sockfd)> NewConnectionCallback;

  Connector(EventLoop* loop, const InetAddress& serverAddr);
  ~Connector();

  /**
   * 设置新建连接回调函数
   */
  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }

  void start();  // can be called in any thread
  void restart();  // must be called in loop thread
  void stop();  // can be called in any thread

  /**
   * 返回服务器地址对象
   */
  const InetAddress& serverAddress() const { return serverAddr_; }

 private:
  /**
   * 状态枚举类型定义
   */
  enum States { kDisconnected, kConnecting, kConnected };
  /**
   * 最大重试延迟时间
   */
  static const int kMaxRetryDelayMs = 30*1000;
  /**
   * 初始重试延迟时间
   */
  static const int kInitRetryDelayMs = 500;

  /**
   * 设置状态
   */
  void setState(States s) { state_ = s; }
  void startInLoop();
  void stopInLoop();
  void connect();
  void connecting(int sockfd);
  void handleWrite();
  void handleError();
  void retry(int sockfd);
  int removeAndResetChannel();
  void resetChannel();

  /**
   * 指向事件循环的指针
   */
  EventLoop* loop_;
  /**
   * 服务器地址对象
   */
  InetAddress serverAddr_;
  /**
   * 是否连接
   */
  bool connect_; // atomic
  /**
   * 状态
   */
  States state_;  // FIXME: use atomic variable
  /**
   * 指向通道的唯一指针
   */
  std::unique_ptr<Channel> channel_;
  /**
   * 新建连接回调函数
   */
  NewConnectionCallback newConnectionCallback_;
  /**
   * 重试延迟时间
   */
  int retryDelayMs_;
};

}
}

#endif  // MUDUO_NET_CONNECTOR_H
