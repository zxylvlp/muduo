// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Timer.h>

using namespace muduo;
using namespace muduo::net;

/**
 * 静态成员最近创建的数的初始化
 */
AtomicInt64 Timer::s_numCreated_;

/**
 * 重启定时器
 *
 * 首先判断是否重复，如果不重复则将过期时间设置为无效
 * 否则将过期时间设置为当前时间加时间间隔
 */
void Timer::restart(Timestamp now)
{
  if (repeat_)
  {
    expiration_ = addTime(now, interval_);
  }
  else
  {
    expiration_ = Timestamp::invalid();
  }
}
