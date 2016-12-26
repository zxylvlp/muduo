// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThread.h>

#include <muduo/net/EventLoop.h>

using namespace muduo;
using namespace muduo::net;

/**
 * 构造函数
 */
EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const string& name)
  : loop_(NULL),
    exiting_(false),
    thread_(std::bind(&EventLoopThread::threadFunc, this), name),
    mutex_(),
    cond_(mutex_),
    callback_(cb)
{
}

/**
 * 析构函数
 *
 * 首先将正在退出设置为真
 * 然后判断事件循环指针是否为空
 * 如果不为空则调用事件循环的退出函数并且等待该线程运行结束
 */
EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
  {
    // still a tiny chance to call destructed object, if threadFunc exits just now.
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    loop_->quit();
    thread_.join();
  }
}

/**
 * 开始循环
 *
 * 首先开启线程
 * 然后等待事件循环指针不为空，并且返回该指针
 */
EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  thread_.start();

  {
    MutexLockGuard lock(mutex_);
    while (loop_ == NULL)
    {
      cond_.wait();
    }
  }

  return loop_;
}

/**
 * 线程函数
 *
 * 首先创建一个事件循环对象
 * 然后调用线程初始化回调函数
 * 并且将事件循环指针指向该对象并唤醒主线程
 * 然后在事件循环中循环
 * 最后将事件循环指针设为空
 */
void EventLoopThread::threadFunc()
{
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);
  }

  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;
    cond_.notify();
  }

  loop.loop();
  //assert(exiting_);
  loop_ = NULL;
}

