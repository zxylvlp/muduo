// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpServer.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Acceptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/SocketsOps.h>

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

/**
 * 构造函数
 *
 * 做初始化工作，这里构建了acceptor和EventLoop线程池
 * 并且给acceptor设置了新建连接回调函数为TcpServer::newConnection
 */
TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const string& nameArg,
                     Option option)
  : loop_(CHECK_NOTNULL(loop)),
    ipPort_(listenAddr.toIpPort()),
    name_(nameArg),
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
    threadPool_(new EventLoopThreadPool(loop, name_)),
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    nextConnId_(1)
{
  acceptor_->setNewConnectionCallback(
      std::bind(&TcpServer::newConnection, this, _1, _2));
}

/**
 * 析构函数
 *
 * 迭代连接map中的连接，将其连接指针置为空，并且将连接在其loop中调用TcpConnection::connectDestroyed函数
 */
TcpServer::~TcpServer()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

  for (ConnectionMap::iterator it(connections_.begin());
      it != connections_.end(); ++it)
  {
    TcpConnectionPtr conn = it->second;
    it->second.reset();
    conn->getLoop()->runInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn));
    conn.reset();
  }
}

/**
 * 设置EventLoop线程池的线程数
 */
void TcpServer::setThreadNum(int numThreads)
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

/**
 * 开启服务器
 *
 * 如果还没开启则做下面的操作：
 * 首先调用EventLoop线程池的开启函数，并且传入线程初始化回调函数
 * 并且在loop线程调用acceptor的listen函数
 */
void TcpServer::start()
{
  if (started_.getAndSet(1) == 0)
  {
    threadPool_->start(threadInitCallback_);

    assert(!acceptor_->listenning());
    loop_->runInLoop(
        std::bind(&Acceptor::listen, get_pointer(acceptor_)));
  }
}

/**
 * 新建连接时的处理函数
 *
 * 从EventLoop线程池中获得一个loop，并且根据服务器名、ip地址和端口号还有下一个连接id创建一个连接名
 * 将下一个连接id加1
 * 利用前面的信息创建一个连接对象，并且把连接对象包裹在智能指针中，利用连接名作为key，将智能指针放到连接map中
 * 设置连接的建立连接回调函数、收到消息回调函数、写完成回调函数和关闭连接回调函数
 * 最后在loop线程里调用TcpConnection::connectEstablished函数
 */
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
  loop_->assertInLoopThread();
  EventLoop* ioLoop = threadPool_->getNextLoop();
  char buf[64];
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
  connections_[connName] = conn;
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
      std::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe
  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

/**
 * 删除连接函数
 *
 * 在服务器loop线程调用TcpServer::removeConnectionInLoop
 */
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  // FIXME: unsafe
  loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

/**
 * loop线程删除连接函数
 *
 * 将连接从连接map中删去，然后获得连接的loop，在其线程中调用TcpConnection::connectDestroyed
 */
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();
  size_t n = connections_.erase(conn->name());
  (void)n;
  assert(n == 1);
  EventLoop* ioLoop = conn->getLoop();
  ioLoop->queueInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn));
}

