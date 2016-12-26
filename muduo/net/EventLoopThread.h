// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOPTHREAD_H
#define MUDUO_NET_EVENTLOOPTHREAD_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>

namespace muduo
{
namespace net
{

class EventLoop;

/**
 * 构造函数
 */
class EventLoopThread : noncopyable
{
 public:
  /**
   * 线程初始化回调函数类型
   */
  typedef std::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                  const string& name = string());
  ~EventLoopThread();
  EventLoop* startLoop();

 private:
  void threadFunc();

  /**
   * 指向事件循环的指针
   */
  EventLoop* loop_;
  /**
   * 是否正在退出
   */
  bool exiting_;
  /**
   * 线程对象
   */
  Thread thread_;
  /**
   * 互斥锁
   */
  MutexLock mutex_;
  /**
   * 条件变量
   */
  Condition cond_;
  /**
   * 线程初始化回调函数
   */
  ThreadInitCallback callback_;
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREAD_H

