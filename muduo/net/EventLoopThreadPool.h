// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_EVENTLOOPTHREADPOOL_H
#define MUDUO_NET_EVENTLOOPTHREADPOOL_H

#include <muduo/base/noncopyable.h>
#include <muduo/base/Types.h>

#include <functional>
#include <memory>
#include <vector>

namespace muduo
{

namespace net
{

class EventLoop;
class EventLoopThread;

/**
 * 事件循环线程池
 */
class EventLoopThreadPool : noncopyable
{
 public:
  /**
   * 定义线程初始化回调函数
   */
  typedef std::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThreadPool(EventLoop* baseLoop, const string& nameArg);
  ~EventLoopThreadPool();
  void setThreadNum(int numThreads) { numThreads_ = numThreads; }
  void start(const ThreadInitCallback& cb = ThreadInitCallback());

  // valid after calling start()
  /// round-robin
  EventLoop* getNextLoop();

  /// with the same hash code, it will always return the same EventLoop
  EventLoop* getLoopForHash(size_t hashCode);

  std::vector<EventLoop*> getAllLoops();

  /**
   * 返回是否已经开始
   */
  bool started() const
  { return started_; }

  /**
   * 返回名字的引用
   */
  const string& name() const
  { return name_; }

 private:

  /**
   * 指向基本事件循环的指针
   */
  EventLoop* baseLoop_;
  /**
   * 名字
   */
  string name_;
  /**
   * 是否开始
   */
  bool started_;
  /**
   * 线程数
   */
  int numThreads_;
  /**
   * 下一个位置
   */
  int next_;
  /**
   * 事件循环线程指针数组
   */
  std::vector<std::unique_ptr<EventLoopThread>> threads_;
  /**
   * 事件循环指针数组
   */
  std::vector<EventLoop*> loops_;
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREADPOOL_H
