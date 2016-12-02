// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMER_H
#define MUDUO_NET_TIMER_H

#include <muduo/base/Atomic.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>

namespace muduo
{
namespace net
{
///
/// Internal class for timer event.
///
/**
 * 定时器类
 */
class Timer : noncopyable
{
 public:
  /**
   * 构造函数
   */
  Timer(const TimerCallback& cb, Timestamp when, double interval)
    : callback_(cb),
      expiration_(when),
      interval_(interval),
      repeat_(interval > 0.0),
      sequence_(s_numCreated_.incrementAndGet())
  { }

#ifdef __GXX_EXPERIMENTAL_CXX0X__
  Timer(TimerCallback&& cb, Timestamp when, double interval)
    : callback_(std::move(cb)),
      expiration_(when),
      interval_(interval),
      repeat_(interval > 0.0),
      sequence_(s_numCreated_.incrementAndGet())
  { }
#endif

  /**
   * 运行
   *
   * 调用构造时传入的回调函数
   */
  void run() const
  {
    callback_();
  }

  /**
   * 返回过期时间
   */
  Timestamp expiration() const  { return expiration_; }
  /**
   * 返回是否重复
   */
  bool repeat() const { return repeat_; }
  /**
   * 返回序列号
   */
  int64_t sequence() const { return sequence_; }

  void restart(Timestamp now);

  /**
   * 返回最近创建的数
   */
  static int64_t numCreated() { return s_numCreated_.get(); }

 private:
  /**
   * 回调函数
   */
  const TimerCallback callback_;
  /**
   * 过期时间
   */
  Timestamp expiration_;
  /**
   * 间隔时间
   */
  const double interval_;
  /**
   * 是否重复
   */
  const bool repeat_;
  /**
   * 序列号
   */
  const int64_t sequence_;

  /**
   * 最近创建的数
   */
  static AtomicInt64 s_numCreated_;
};
}
}
#endif  // MUDUO_NET_TIMER_H
