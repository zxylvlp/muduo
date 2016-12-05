// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOP_H
#define MUDUO_NET_EVENTLOOP_H

#include <functional>
#include <vector>

#include <boost/any.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/base/CurrentThread.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/TimerId.h>

namespace muduo
{
namespace net
{

class Channel;
class Poller;
class TimerQueue;

///
/// Reactor, at most one per thread.
///
/// This is an interface class, so don't expose too much details.
/**
 * 事件循环类
 */
class EventLoop : noncopyable
{
 public:
  /**
   * 定义函数类型
   */
  typedef std::function<void()> Functor;

  EventLoop();
  ~EventLoop();  // force out-line dtor, for std::unique_ptr members.

  ///
  /// Loops forever.
  ///
  /// Must be called in the same thread as creation of the object.
  ///
  void loop();

  /// Quits loop.
  ///
  /// This is not 100% thread safe, if you call through a raw pointer,
  /// better to call through shared_ptr<EventLoop> for 100% safety.
  void quit();

  ///
  /// Time when poll returns, usually means data arrival.
  ///
  /**
   * 返回轮询返回的时间
   */
  Timestamp pollReturnTime() const { return pollReturnTime_; }

  /**
   * 返回循环次数
   */
  int64_t iteration() const { return iteration_; }

  /// Runs callback immediately in the loop thread.
  /// It wakes up the loop, and run the cb.
  /// If in the same loop thread, cb is run within the function.
  /// Safe to call from other threads.
  void runInLoop(const Functor& cb);
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
  void queueInLoop(const Functor& cb);

  size_t queueSize() const;

#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void runInLoop(Functor&& cb);
  void queueInLoop(Functor&& cb);
#endif

  // timers

  ///
  /// Runs callback at 'time'.
  /// Safe to call from other threads.
  ///
  TimerId runAt(const Timestamp& time, const TimerCallback& cb);
  ///
  /// Runs callback after @c delay seconds.
  /// Safe to call from other threads.
  ///
  TimerId runAfter(double delay, const TimerCallback& cb);
  ///
  /// Runs callback every @c interval seconds.
  /// Safe to call from other threads.
  ///
  TimerId runEvery(double interval, const TimerCallback& cb);
  ///
  /// Cancels the timer.
  /// Safe to call from other threads.
  ///
  void cancel(TimerId timerId);

#ifdef __GXX_EXPERIMENTAL_CXX0X__
  TimerId runAt(const Timestamp& time, TimerCallback&& cb);
  TimerId runAfter(double delay, TimerCallback&& cb);
  TimerId runEvery(double interval, TimerCallback&& cb);
#endif

  // internal usage
  void wakeup();
  void updateChannel(Channel* channel);
  void removeChannel(Channel* channel);
  bool hasChannel(Channel* channel);

  // pid_t threadId() const { return threadId_; }
  /**
   * 断言位于循环线程
   *
   * 判断是否位于循环线程，如果是则直接返回
   * 否则退出
   */
  void assertInLoopThread()
  {
    if (!isInLoopThread())
    {
      abortNotInLoopThread();
    }
  }
  /**
   * 当前线程号是否等于本事件循环的线程号
   */
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  // bool callingPendingFunctors() const { return callingPendingFunctors_; }
  /**
   * 返回是否正在处理事件
   */
  bool eventHandling() const { return eventHandling_; }

  /**
   * 设置上下文
   */
  void setContext(const boost::any& context)
  { context_ = context; }

  /**
   * 返回上下文
   */
  const boost::any& getContext() const
  { return context_; }

  /**
   * 返回可变上下文
   */
  boost::any* getMutableContext()
  { return &context_; }

  static EventLoop* getEventLoopOfCurrentThread();

 private:
  void abortNotInLoopThread();
  void handleRead();  // waked up
  void doPendingFunctors();

  void printActiveChannels() const; // DEBUG

  /**
   * 定义通道列表类型为指向通道指针的数组
   */
  typedef std::vector<Channel*> ChannelList;

  /**
   * 是否正在循环
   */
  bool looping_; /* atomic */
  /**
   * 是否已经标记退出
   */
  bool quit_; /* atomic and shared between threads, okay on x86, I guess. */
  /**
   * 是否正在处理事件
   */
  bool eventHandling_; /* atomic */
  /**
   * 是否正在调用等待的函数
   */
  bool callingPendingFunctors_; /* atomic */
  /**
   * 循环次数
   */
  int64_t iteration_;
  /**
   * 本事件循环的线程号
   */
  const pid_t threadId_;
  /**
   * 轮询返回的时间
   */
  Timestamp pollReturnTime_;
  /**
   * 指向轮询器的指针
   */
  std::unique_ptr<Poller> poller_;
  /**
   * 指向计时器队列的指针
   */
  std::unique_ptr<TimerQueue> timerQueue_;
  /**
   * 唤醒描述符
   */
  int wakeupFd_;
  // unlike in TimerQueue, which is an internal class,
  // we don't expose Channel to client.
  /**
   * 指向唤醒描述符的通道的指针
   */
  std::unique_ptr<Channel> wakeupChannel_;
  /**
   * 指向上下文的指针
   */
  boost::any context_;

  // scratch variables
  /**
   * 活跃通道列表
   */
  ChannelList activeChannels_;
  /**
   * 指向当前活跃通道的指针
   */
  Channel* currentActiveChannel_;

  /**
   * 互斥锁
   */
  mutable MutexLock mutex_;
  /**
   * 正在等待的函数数组
   */
  std::vector<Functor> pendingFunctors_; // @GuardedBy mutex_
};

}
}
#endif  // MUDUO_NET_EVENTLOOP_H
