// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Poller.h>
#include <muduo/net/poller/PollPoller.h>
#include <muduo/net/poller/EPollPoller.h>

#include <stdlib.h>

using namespace muduo::net;

/**
 * 创建默认轮询器
 *
 * 根据环境参数选择是创建普通轮询器还是e轮询器
 */
Poller* Poller::newDefaultPoller(EventLoop* loop)
{
  if (::getenv("MUDUO_USE_POLL"))
  {
    return new PollPoller(loop);
  }
  else
  {
    return new EPollPoller(loop);
  }
}
