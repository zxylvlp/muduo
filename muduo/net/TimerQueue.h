// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMERQUEUE_H
#define MUDUO_NET_TIMERQUEUE_H

#include <set>
#include <vector>

#include <muduo/base/Mutex.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Channel.h>

namespace muduo
{
namespace net
{

class EventLoop;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
///
/**
 * 计时器队列
 */
class TimerQueue : noncopyable
{
 public:
  TimerQueue(EventLoop* loop);
  ~TimerQueue();

  ///
  /// Schedules the callback to be run at given time,
  /// repeats if @c interval > 0.0.
  ///
  /// Must be thread safe. Usually be called from other threads.
  TimerId addTimer(const TimerCallback& cb,
                   Timestamp when,
                   double interval);
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  TimerId addTimer(TimerCallback&& cb,
                   Timestamp when,
                   double interval);
#endif

  void cancel(TimerId timerId);

 private:

  // FIXME: use unique_ptr<Timer> instead of raw pointers.
  /**
   * 定义时间戳和定制器指针对为元素类型
   */
  typedef std::pair<Timestamp, Timer*> Entry;
  /**
   * 定义元素集合为定时器列表类型
   */
  typedef std::set<Entry> TimerList;
  /**
   * 定义定时器指针和序列号对为活跃定时器类型
   */
  typedef std::pair<Timer*, int64_t> ActiveTimer;
  /**
   * 定义活跃定时器集合为活跃定时器结合类型
   */
  typedef std::set<ActiveTimer> ActiveTimerSet;

  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId);
  // called when timerfd alarms
  void handleRead();
  // move out all expired timers
  std::vector<Entry> getExpired(Timestamp now);
  void reset(const std::vector<Entry>& expired, Timestamp now);

  bool insert(Timer* timer);

  /**
   * 指向事件循环的指针
   */
  EventLoop* loop_;
  /**
   * 定时器文件描述符
   */
  const int timerfd_;
  /**
   * 定时器文件描述符通道
   */
  Channel timerfdChannel_;
  // Timer list sorted by expiration
  /**
   * 定时器列表
   */
  TimerList timers_;

  // for cancel()
  /**
   * 活跃的定时器集合
   */
  ActiveTimerSet activeTimers_;
  /**
   * 正在调用到期的计时器的回调
   */
  bool callingExpiredTimers_; /* atomic */
  /**
   * 正在取消的定时器集合
   */
  ActiveTimerSet cancelingTimers_;
};

}
}
#endif  // MUDUO_NET_TIMERQUEUE_H
