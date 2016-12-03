// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include <muduo/base/Timestamp.h>

#include <functional>
#include <memory>

namespace muduo
{
namespace net
{

class EventLoop;

///
/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor.
/// The file descriptor could be a socket,
/// an eventfd, a timerfd, or a signalfd
/**
 * 通道类
 */
class Channel : noncopyable
{
 public:
  /**
   * 定义事件回调函数类型
   */
  typedef std::function<void()> EventCallback;
  /**
   * 定义读事件回调函数类型
   */
  typedef std::function<void(Timestamp)> ReadEventCallback;

  Channel(EventLoop* loop, int fd);
  ~Channel();

  void handleEvent(Timestamp receiveTime);
  /**
   * 设置读回调函数
   */
  void setReadCallback(const ReadEventCallback& cb)
  { readCallback_ = cb; }
  /**
   * 设置写回调函数
   */
  void setWriteCallback(const EventCallback& cb)
  { writeCallback_ = cb; }
  /**
   * 设置关闭回调函数
   */
  void setCloseCallback(const EventCallback& cb)
  { closeCallback_ = cb; }
  /**
   * 设置错误回调函数
   */
  void setErrorCallback(const EventCallback& cb)
  { errorCallback_ = cb; }
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void setReadCallback(ReadEventCallback&& cb)
  { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback&& cb)
  { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback&& cb)
  { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback&& cb)
  { errorCallback_ = std::move(cb); }
#endif

  /// Tie this channel to the owner object managed by shared_ptr,
  /// prevent the owner object being destroyed in handleEvent.
  void tie(const std::shared_ptr<void>&);

  /**
   * 返回文件描述符
   */
  int fd() const { return fd_; }
  /**
   * 返回允许的事件
   */
  int events() const { return events_; }
  /**
   * 返回收到的事件
   */
  void set_revents(int revt) { revents_ = revt; } // used by pollers
  // int revents() const { return revents_; }
  /**
   * 是否什么事件都不允许
   */
  bool isNoneEvent() const { return events_ == kNoneEvent; }

  /**
   * 允许读事件
   *
   * 首先将允许的事件中加入读，然后调用update更新文件描述符的事件
   */
  void enableReading() { events_ |= kReadEvent; update(); }

  /**
   * 取消读事件
   *
   * 首先将允许的事件中取消读，然后调用update更新文件描述符的事件
   */
  void disableReading() { events_ &= ~kReadEvent; update(); }
  /**
   * 允许写事件
   *
   * 首先将允许的事件中加入写，然后调用update更新文件描述符的事件
   */
  void enableWriting() { events_ |= kWriteEvent; update(); }
  /**
   * 取消写事件
   *
   * 首先将允许的事件中取消写，然后调用update更新文件描述符的事件
   */
  void disableWriting() { events_ &= ~kWriteEvent; update(); }
  /**
   * 取消所有事件
   *
   * 首先将允许的事件清空，然后调用update更新文件描述符的事件
   */
  void disableAll() { events_ = kNoneEvent; update(); }
  /**
   * 返回是否允许写事件
   */
  bool isWriting() const { return events_ & kWriteEvent; }
  /**
   * 返回是否允许读事件
   */
  bool isReading() const { return events_ & kReadEvent; }

  // for Poller
  /**
   * 返回通道的状态是新添加，已删除还是已添加
   */
  int index() { return index_; }
  /**
   * 设置通道的状态是新添加，已删除还是已添加
   */
  void set_index(int idx) { index_ = idx; }

  // for debug
  string reventsToString() const;
  string eventsToString() const;

  /**
   * 取消将hup事件记录日志
   */
  void doNotLogHup() { logHup_ = false; }

  /**
   * 返回属于的事件循环
   */
  EventLoop* ownerLoop() { return loop_; }
  void remove();

 private:
  static string eventsToString(int fd, int ev);

  void update();
  void handleEventWithGuard(Timestamp receiveTime);

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  /**
   * 指向事件循环的指针
   */
  EventLoop* loop_;
  /**
   * 文件描述符
   */
  const int  fd_;
  /**
   * 允许的事件
   */
  int        events_;
  /**
   * 接收到的事件
   */
  int        revents_; // it's the received event types of epoll or poll
  /**
   * 通道的状态，有：新添加，已删除和已添加
   */
  int        index_; // used by Poller.
  /**
   * 是否允许将hup事件记录日志
   */
  bool       logHup_;

  /**
   * 指向绑定对象的指针
   */
  std::weak_ptr<void> tie_;
  /**
   * 是否被绑定
   */
  bool tied_;
  /**
   * 是否正在处理事件
   */
  bool eventHandling_;
  /**
   * 文件描述符是否加入事件循环
   */
  bool addedToLoop_;
  /**
   * 读回调函数
   */
  ReadEventCallback readCallback_;
  /**
   * 写回调函数
   */
  EventCallback writeCallback_;
  /**
   * 关闭回调函数
   */
  EventCallback closeCallback_;
  /**
   * 出错回调函数
   */
  EventCallback errorCallback_;
};

}
}
#endif  // MUDUO_NET_CHANNEL_H
