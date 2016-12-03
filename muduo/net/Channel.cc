// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

/**
 * 没有事件
 */
const int Channel::kNoneEvent = 0;
/**
 * 读事件
 */
const int Channel::kReadEvent = POLLIN | POLLPRI;
/**
 * 写事件
 */
const int Channel::kWriteEvent = POLLOUT;

/**
 * 构造函数
 */
Channel::Channel(EventLoop* loop, int fd__)
  : loop_(loop),
    fd_(fd__),
    events_(0),
    revents_(0),
    index_(-1),
    logHup_(true),
    tied_(false),
    eventHandling_(false),
    addedToLoop_(false)
{
}

/**
 * 析构函数
 */
Channel::~Channel()
{
  assert(!eventHandling_);
  assert(!addedToLoop_);
  if (loop_->isInLoopThread())
  {
    assert(!loop_->hasChannel(this));
  }
}

/**
 * 绑定到连接
 */
void Channel::tie(const std::shared_ptr<void>& obj)
{
  tie_ = obj;
  tied_ = true;
}

/**
 * 更新
 *
 * 将文件描述符是否加入事件循环设置为真
 * 调用所在事件循环的更新通道函数
 */
void Channel::update()
{
  addedToLoop_ = true;
  loop_->updateChannel(this);
}

/**
 * 删除
 *
 * 将文件描述符是否加入事件循环设置为假
 * 调用所在事件循环的删除通道函数
 */
void Channel::remove()
{
  assert(isNoneEvent());
  addedToLoop_ = false;
  loop_->removeChannel(this);
}

/**
 * 处理事件
 *
 * 如果绑定过对象，则将那个对象的引用计数加1后调用handleEventWithGuard实际处理事件
 * 否则直接调用handleEventWithGuard实际处理事件
 */
void Channel::handleEvent(Timestamp receiveTime)
{
  std::shared_ptr<void> guard;
  if (tied_)
  {
    guard = tie_.lock();
    if (guard)
    {
      handleEventWithGuard(receiveTime);
    }
  }
  else
  {
    handleEventWithGuard(receiveTime);
  }
}

/**
 * 实际处理事件
 *
 * 首先将正在处理事件设置为真
 * 判断是否是hup事件并且没有数据可读则调用关闭回调
 * 判断是否是nval或者出错事件则调用出错回调
 * 判断是否是可读事件如果是则调用可读回调
 * 判断是否是可写事件如果是则调用可写回调
 * 最后将正在处理事件设置为假
 */
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
  eventHandling_ = true;
  LOG_TRACE << reventsToString();
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
  {
    if (logHup_)
    {
      LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
    }
    if (closeCallback_) closeCallback_();
  }

  if (revents_ & POLLNVAL)
  {
    LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
  }

  if (revents_ & (POLLERR | POLLNVAL))
  {
    if (errorCallback_) errorCallback_();
  }
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))
  {
    if (readCallback_) readCallback_(receiveTime);
  }
  if (revents_ & POLLOUT)
  {
    if (writeCallback_) writeCallback_();
  }
  eventHandling_ = false;
}

/**
 * 将接受到的事件转换为字符串
 */
string Channel::reventsToString() const
{
  return eventsToString(fd_, revents_);
}

/**
 * 将允许接受的事件转化为字符串
 */
string Channel::eventsToString() const
{
  return eventsToString(fd_, events_);
}

/**
 * 将事件转换为字符串
 */
string Channel::eventsToString(int fd, int ev)
{
  std::ostringstream oss;
  oss << fd << ": ";
  if (ev & POLLIN)
    oss << "IN ";
  if (ev & POLLPRI)
    oss << "PRI ";
  if (ev & POLLOUT)
    oss << "OUT ";
  if (ev & POLLHUP)
    oss << "HUP ";
  if (ev & POLLRDHUP)
    oss << "RDHUP ";
  if (ev & POLLERR)
    oss << "ERR ";
  if (ev & POLLNVAL)
    oss << "NVAL ";

  return oss.str().c_str();
}
