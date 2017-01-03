// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpConnection.h>

#include <muduo/base/Logging.h>
#include <muduo/base/WeakCallback.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Socket.h>
#include <muduo/net/SocketsOps.h>

#include <errno.h>

using namespace muduo;
using namespace muduo::net;

/**
 * 默认连接成功回调函数
 *
 * 打印trace级别的日志，内容为连接信息
 */
void muduo::net::defaultConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
  // do not call conn->forceClose(), because some users want to register message callback only.
}

/**
 * 默认收到消息回调函数
 *
 * 读取所有信息
 */
void muduo::net::defaultMessageCallback(const TcpConnectionPtr&,
                                        Buffer* buf,
                                        Timestamp)
{
  buf->retrieveAll();
}

/**
 * 构造函数
 *
 * 首先构造初始化本连接的属性
 * 然后对其channel设置读、写、关闭和出错处理函数
 * 最后对其socket设置keepalive属性
 */
TcpConnection::TcpConnection(EventLoop* loop,
                             const string& nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
  : loop_(CHECK_NOTNULL(loop)),
    name_(nameArg),
    state_(kConnecting),
    socket_(new Socket(sockfd)),
    channel_(new Channel(loop, sockfd)),
    localAddr_(localAddr),
    peerAddr_(peerAddr),
    highWaterMark_(64*1024*1024),
    reading_(true)
{
  channel_->setReadCallback(
      std::bind(&TcpConnection::handleRead, this, _1));
  channel_->setWriteCallback(
      std::bind(&TcpConnection::handleWrite, this));
  channel_->setCloseCallback(
      std::bind(&TcpConnection::handleClose, this));
  channel_->setErrorCallback(
      std::bind(&TcpConnection::handleError, this));
  LOG_DEBUG << "TcpConnection::ctor[" <<  name_ << "] at " << this
            << " fd=" << sockfd;
  socket_->setKeepAlive(true);
}

/**
 * 析构函数
 *
 * 打印debug级别日志
 */
TcpConnection::~TcpConnection()
{
  LOG_DEBUG << "TcpConnection::dtor[" <<  name_ << "] at " << this
            << " fd=" << channel_->fd()
            << " state=" << stateToString();
  assert(state_ == kDisconnected);
}

/**
 * 获取tcp信息
 *
 * 从自己的socket中获取
 */
bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const
{
  return socket_->getTcpInfo(tcpi);
}

/**
 * 获取字符串形式的tcp信息
 *
 * 从自己的socket中获取
 */
string TcpConnection::getTcpInfoString() const
{
  char buf[1024];
  buf[0] = '\0';
  socket_->getTcpInfoString(buf, sizeof buf);
  return buf;
}

/**
 * 发送数据
 *
 * 将指针和长度封装成StringPiece之后调用send重载
 */
void TcpConnection::send(const void* data, int len)
{
  send(StringPiece(static_cast<const char*>(data), len));
}

/**
 * 发送数据
 *
 * 首先判断是否已经连接，如果不是直接返回
 * 然后判断是否在其所在loop的线程中，如果是调用sendInLoop发送数据
 * 如果不是将sendInLoop发送到loop的线程的函数队列中去执行
 */
void TcpConnection::send(const StringPiece& message)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(message);
    }
    else
    {
      void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop;
      loop_->runInLoop(
          std::bind(fp,
                    this,     // FIXME
                    message.as_string()));
                    //std::forward<string>(message)));
    }
  }
}

// FIXME efficiency!!!
/**
 * 发送数据
 *
 * 首先判断是否已经连接，如果不是直接返回
 * 然后判断是否在其所在loop的线程中，如果是调用sendInLoop发送所有可读数据，将buffer置空
 * 如果不是将buffer中可读数据提取成字符串，将buffer置空，在将sendInLoop发送到loop的线程的函数队列中去执行
 */
void TcpConnection::send(Buffer* buf)
{
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(buf->peek(), buf->readableBytes());
      buf->retrieveAll();
    }
    else
    {
      void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop;
      loop_->runInLoop(
          std::bind(fp,
                    this,     // FIXME
                    buf->retrieveAllAsString()));
                    //std::forward<string>(message)));
    }
  }
}

/**
 * 在loop中发送数据
 *
 * 将StringPiece转换成指针和长度调用重载
 */
void TcpConnection::sendInLoop(const StringPiece& message)
{
  sendInLoop(message.data(), message.size());
}

/**
 * 在loop中发送数据
 *
 * 首先判断连接状态是否是断开，如果是直接返回
 * 然后判断是否channel没有开启写事件，并且输出buffer为空，如果是进行如下操作：
 * 首先调用尝试直接写到fd中，看看其返回值，如果大于等于0，说明写成功了，否则说明写失败了
 * 写成功的话判断是否完全写进去了，如果是则调用写完成回调函数
 * 写失败的话判断错误码是否是EWOULDBLOCK，如果是则忽略，然后判断错误是否是EPIPE或者ECONNRESET，如果是则认为是fault错误
 *
 * 如果不是fault错误，说明还要继续发送数据，如果是则直接返回
 * 判断将本次剩下要写的加入输出buffer是否到达高水位，如果到达则将高水位回调添加到调用队列中
 * 然后将剩下要写的加入输出buffer
 * 最后判断channel是否开启写事件，如果没有则开启其写事件
 */
void TcpConnection::sendInLoop(const void* data, size_t len)
{
  loop_->assertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool faultError = false;
  if (state_ == kDisconnected)
  {
    LOG_WARN << "disconnected, give up writing";
    return;
  }
  // if no thing in output queue, try writing directly
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
  {
    nwrote = sockets::write(channel_->fd(), data, len);
    if (nwrote >= 0)
    {
      remaining = len - nwrote;
      if (remaining == 0 && writeCompleteCallback_)
      {
        loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
      }
    }
    else // nwrote < 0
    {
      nwrote = 0;
      if (errno != EWOULDBLOCK)
      {
        LOG_SYSERR << "TcpConnection::sendInLoop";
        if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
        {
          faultError = true;
        }
      }
    }
  }

  assert(remaining <= len);
  if (!faultError && remaining > 0)
  {
    size_t oldLen = outputBuffer_.readableBytes();
    if (oldLen + remaining >= highWaterMark_
        && oldLen < highWaterMark_
        && highWaterMarkCallback_)
    {
      loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
    }
    outputBuffer_.append(static_cast<const char*>(data)+nwrote, remaining);
    if (!channel_->isWriting())
    {
      channel_->enableWriting();
    }
  }
}

/**
 * shutdown函数
 *
 * 判断是否处于连接状态，如果不是直接返回
 * 将状态设置为断开中
 * 然后在loop线程中调用shutdownInLoop
 */
void TcpConnection::shutdown()
{
  // FIXME: use compare and swap
  if (state_ == kConnected)
  {
    setState(kDisconnecting);
    // FIXME: shared_from_this()?
    loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
  }
}

/**
 * 在loop线程中的shuntdown函数
 *
 * 判断是否channel开启写事件，如果开启了则返回
 * 如果没有开启则调用socket的shuntdownWrite
 */
void TcpConnection::shutdownInLoop()
{
  loop_->assertInLoopThread();
  if (!channel_->isWriting())
  {
    // we are not writing
    socket_->shutdownWrite();
  }
}

// void TcpConnection::shutdownAndForceCloseAfter(double seconds)
// {
//   // FIXME: use compare and swap
//   if (state_ == kConnected)
//   {
//     setState(kDisconnecting);
//     loop_->runInLoop(std::bind(&TcpConnection::shutdownAndForceCloseInLoop, this, seconds));
//   }
// }

// void TcpConnection::shutdownAndForceCloseInLoop(double seconds)
// {
//   loop_->assertInLoopThread();
//   if (!channel_->isWriting())
//   {
//     // we are not writing
//     socket_->shutdownWrite();
//   }
//   loop_->runAfter(
//       seconds,
//       makeWeakCallback(shared_from_this(),
//                        &TcpConnection::forceCloseInLoop));
// }

/**
 * 强制关闭
 *
 * 判断状态是否为已连接或者正在关闭如果不是则直接返回
 * 否则将状态设置为正在关闭
 * 然后在事件循环中调用forceCloseInLoop函数
 */
void TcpConnection::forceClose()
{
  // FIXME: use compare and swap
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    setState(kDisconnecting);
    loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
  }
}

/**
 * 延迟几秒强制关闭
 *
 * 判断状态是否为已连接或者正在关闭如果不是则直接返回
 * 否则将状态设置为正在关闭
 * 延迟几秒后在事件循环线程调用强制关闭
 */
void TcpConnection::forceCloseWithDelay(double seconds)
{
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    setState(kDisconnecting);
    loop_->runAfter(
        seconds,
        makeWeakCallback(shared_from_this(),
                         &TcpConnection::forceClose));  // not forceCloseInLoop to avoid race condition
  }
}

/**
 * 在事件循环线程中强制关闭
 *
 * 判断状态是否为已连接或者正在关闭如果不是则直接返回
 * 否则将状态设置为正在关闭
 * 调用处理关闭函数
 */
void TcpConnection::forceCloseInLoop()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    // as if we received 0 byte in handleRead();
    handleClose();
  }
}

/**
 * 将状态转换为字符串
 */
const char* TcpConnection::stateToString() const
{
  switch (state_)
  {
    case kDisconnected:
      return "kDisconnected";
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    default:
      return "unknown state";
  }
}

/**
 * 设置tcp不延迟
 */
void TcpConnection::setTcpNoDelay(bool on)
{
  socket_->setTcpNoDelay(on);
}

/**
 * 开始读
 *
 * 在事件循环线程中开始读
 */
void TcpConnection::startRead()
{
  loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop, this));
}

/**
 * 在事件循环线程中开始读
 *
 * 如果不是正在读或通道没有允许读则允许通道读并且将正在读设置为真
 */
void TcpConnection::startReadInLoop()
{
  loop_->assertInLoopThread();
  if (!reading_ || !channel_->isReading())
  {
    channel_->enableReading();
    reading_ = true;
  }
}

/**
 * 停止读
 *
 * 在事件循环线程中停止读
 */
void TcpConnection::stopRead()
{
  loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, this));
}

/**
 * 在事件循环线程中停止读
 *
 * 如果正在读或者通道允许读则不允许通道读并且将正在读设置为假
 */
void TcpConnection::stopReadInLoop()
{
  loop_->assertInLoopThread();
  if (reading_ || channel_->isReading())
  {
    channel_->disableReading();
    reading_ = false;
  }
}

/**
 * 连接建立
 *
 * 将状态设置为已连接
 * 并将当前对象绑到通道上
 * 允许通道的读并调用连接回调
 */
void TcpConnection::connectEstablished()
{
  loop_->assertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);
  channel_->tie(shared_from_this());
  channel_->enableReading();

  connectionCallback_(shared_from_this());
}

/**
 * 连接销毁
 *
 * 如果状态为已连接则进行以下操作：
 * 设置状态为已断开并关闭通道的任何事件
 * 调用连接回调
 *
 * 最后调用通道的删除函数
 */
void TcpConnection::connectDestroyed()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected)
  {
    setState(kDisconnected);
    channel_->disableAll();

    connectionCallback_(shared_from_this());
  }
  channel_->remove();
}

/**
 * 处理读
 *
 * 读取描述符内容到输入缓冲区
 * 如果长度大于0则调用消息回调
 * 如果长度等于0则调用处理关闭
 * 否则调用处理出错
 */
void TcpConnection::handleRead(Timestamp receiveTime)
{
  loop_->assertInLoopThread();
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0)
  {
    messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
  }
  else if (n == 0)
  {
    handleClose();
  }
  else
  {
    errno = savedErrno;
    LOG_SYSERR << "TcpConnection::handleRead";
    handleError();
  }
}

/**
 * 处理写
 *
 * 如果通道不允许写则直接返回
 * 将输出缓冲区的内容写入描述符
 * 如果写入长度不大于0则直接返回
 * 如果输出缓冲器还有要写的内容则直接返回
 * 否则关闭通道写事件，在事件循环中调用写完成回调函数
 * 如果状态为断开中调用关闭函数
 */
void TcpConnection::handleWrite()
{
  loop_->assertInLoopThread();
  if (channel_->isWriting())
  {
    ssize_t n = sockets::write(channel_->fd(),
                               outputBuffer_.peek(),
                               outputBuffer_.readableBytes());
    if (n > 0)
    {
      outputBuffer_.retrieve(n);
      if (outputBuffer_.readableBytes() == 0)
      {
        channel_->disableWriting();
        if (writeCompleteCallback_)
        {
          loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
        }
        if (state_ == kDisconnecting)
        {
          shutdownInLoop();
        }
      }
    }
    else
    {
      LOG_SYSERR << "TcpConnection::handleWrite";
      // if (state_ == kDisconnecting)
      // {
      //   shutdownInLoop();
      // }
    }
  }
  else
  {
    LOG_TRACE << "Connection fd = " << channel_->fd()
              << " is down, no more writing";
  }
}

/**
 * 处理关闭
 *
 * 将状态设置为已关闭
 * 禁止通道的所有事件
 * 调用连接回调和关闭回调
 */
void TcpConnection::handleClose()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "fd = " << channel_->fd() << " state = " << stateToString();
  assert(state_ == kConnected || state_ == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  setState(kDisconnected);
  channel_->disableAll();

  TcpConnectionPtr guardThis(shared_from_this());
  connectionCallback_(guardThis);
  // must be the last line
  closeCallback_(guardThis);
}

/**
 * 处理出错
 *
 * 获取并打印错误码
 */
void TcpConnection::handleError()
{
  int err = sockets::getSocketError(channel_->fd());
  LOG_ERROR << "TcpConnection::handleError [" << name_
            << "] - SO_ERROR = " << err << " " << strerror_tl(err);
}

