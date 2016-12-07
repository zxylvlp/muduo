// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Poller.h>

#include <muduo/net/Channel.h>

using namespace muduo;
using namespace muduo::net;

/**
 * 构造函数
 */
Poller::Poller(EventLoop* loop)
  : ownerLoop_(loop)
{
}

/**
 * 析构函数
 */
Poller::~Poller()
{
}

/**
 * 判断轮询器是否有通道
 *
 * 从通道映射中利用通道描述符查找
 */
bool Poller::hasChannel(Channel* channel) const
{
  assertInLoopThread();
  ChannelMap::const_iterator it = channels_.find(channel->fd());
  return it != channels_.end() && it->second == channel;
}

