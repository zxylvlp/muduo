// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>

#include <memory>

#include <boost/any.hpp>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
/**
 * TCP连接类
 */
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
 public:
  /// Constructs a TcpConnection with a connected sockfd
  ///
  /// User should not create this object.
  TcpConnection(EventLoop* loop,
                const string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
  ~TcpConnection();

  /**
   * 获取本连接属于的EventLoop的指针
   */
  EventLoop* getLoop() const { return loop_; }
  /**
   * 获取连接名
   */
  const string& name() const { return name_; }
  /**
   * 获取本地网络地址
   */
  const InetAddress& localAddress() const { return localAddr_; }
  /**
   * 获取对方网络地址
   */
  const InetAddress& peerAddress() const { return peerAddr_; }
  /**
   * 判断当前状态是否为连接状态
   */
  bool connected() const { return state_ == kConnected; }
  /**
   * 判断当前状态是否为断开状态
   */
  bool disconnected() const { return state_ == kDisconnected; }
  // return true if success.
  bool getTcpInfo(struct tcp_info*) const;
  string getTcpInfoString() const;

  // void send(string&& message); // C++11
  void send(const void* message, int len);
  void send(const StringPiece& message);
  // void send(Buffer&& message); // C++11
  void send(Buffer* message);  // this one will swap data
  void shutdown(); // NOT thread safe, no simultaneous calling
  // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
  void forceClose();
  void forceCloseWithDelay(double seconds);
  void setTcpNoDelay(bool on);
  void startRead();
  void stopRead();
  /**
   * 判断是否正在读取
   */
  bool isReading() const { return reading_; }; // NOT thread safe, may race with start/stopReadInLoop 

  /**
   * 设置上下文
   */
  void setContext(const boost::any& context)
  { context_ = context; }

  /**
   * 获取上下文
   */
  const boost::any& getContext() const
  { return context_; }

  /**
   * 获取可变上下文
   */
  boost::any* getMutableContext()
  { return &context_; }

  /**
   * 设置连接成功回调函数
   */
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  /**
   * 设置收到消息回调函数
   */
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  /**
   * 设置写完成的回调函数
   */
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

  /**
   * 设置高水位回调函数和高水位值
   */
  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
  { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

  /// Advanced interface
  /**
   * 返回输入buffer的指针
   */
  Buffer* inputBuffer()
  { return &inputBuffer_; }

  /**
   * 返回输出buffer的指针
   */
  Buffer* outputBuffer()
  { return &outputBuffer_; }

  /// Internal use only.
  /**
   * 设置关闭连接的回调函数
   */
  void setCloseCallback(const CloseCallback& cb)
  { closeCallback_ = cb; }

  // called when TcpServer accepts a new connection
  void connectEstablished();   // should be called only once
  // called when TcpServer has removed me from its map
  void connectDestroyed();  // should be called only once

 private:
  /**
   * 当前连接的状态信息
   */
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();
  // void sendInLoop(string&& message);
  void sendInLoop(const StringPiece& message);
  void sendInLoop(const void* message, size_t len);
  void shutdownInLoop();
  // void shutdownAndForceCloseInLoop(double seconds);
  void forceCloseInLoop();
  /**
   * 设置连接的当前状态
   */
  void setState(StateE s) { state_ = s; }
  const char* stateToString() const;
  void startReadInLoop();
  void stopReadInLoop();

  /**
   * 本连接属于的EventLoop的指针
   */
  EventLoop* loop_;
  /**
   * 连接名
   */
  const string name_;
  /**
   * 连接状态
   */
  StateE state_;  // FIXME: use atomic variable
  // we don't expose those classes to client.
  /**
   * 指向本连接的socket的unique指针
   */
  std::unique_ptr<Socket> socket_;
  /**
   * 指向本连接的channel的unique指针
   */
  std::unique_ptr<Channel> channel_;
  /**
   * 本地网络地址
   */
  const InetAddress localAddr_;
  /**
   * 对方网络地址
   */
  const InetAddress peerAddr_;
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
   * 高水位回调函数
   */
  HighWaterMarkCallback highWaterMarkCallback_;
  /**
   * 关闭连接回调函数
   */
  CloseCallback closeCallback_;
  /**
   * 高水位线
   */
  size_t highWaterMark_;
  /**
   * 输入buffer
   */
  Buffer inputBuffer_;
  /**
   * 输出buffer
   */
  Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer.
  /**
   * 上下文对象
   */
  boost::any context_;
  /**
   * 是否正在读
   */
  bool reading_;
  // FIXME: creationTime_, lastReceiveTime_
  //        bytesReceived_, bytesSent_
};

/**
 * 类型定义指向TcpConnection的智能指针
 */
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

}
}

#endif  // MUDUO_NET_TCPCONNECTION_H
