// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoop.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/Channel.h>
#include <muduo/net/Poller.h>
#include <muduo/net/SocketsOps.h>
#include <muduo/net/TimerQueue.h>

#include <algorithm>

#include <signal.h>
#include <sys/eventfd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{
/**
 * 指向这个线程的事件循环的指针
 */
__thread EventLoop* t_loopInThisThread = 0;

/**
 * 设置轮询的超时时间为10秒
 */
const int kPollTimeMs = 10000;

/**
 * 创建事件描述符
 */
int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
/**
 * 忽略PIPE信号类
 */
class IgnoreSigPipe
{
 public:
  /**
   * 构造函数
   *
   * 忽略PIPE信号
   */
  IgnoreSigPipe()
  {
    ::signal(SIGPIPE, SIG_IGN);
    // LOG_TRACE << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

/**
 * 忽略PIPE信号对象
 */
IgnoreSigPipe initObj;
}

/**
 * 获得当前线程的事件循环
 */
EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

/**
 * 构造函数
 *
 * 首先初始化成员
 * 然后将本对象的指针传递给本线程的事件循环线程变量
 * 最后将唤醒通道的读回调设置为处理读并且允许读事件
 */
EventLoop::EventLoop()
  : looping_(false),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    iteration_(0),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    timerQueue_(new TimerQueue(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)),
    currentActiveChannel_(NULL)
{
  LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
  if (t_loopInThisThread)
  {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  }
  else
  {
    t_loopInThisThread = this;
  }
  wakeupChannel_->setReadCallback(
      std::bind(&EventLoop::handleRead, this));
  // we are always reading the wakeupfd
  wakeupChannel_->enableReading();
}

/**
 * 析构函数
 *
 * 首先取消唤醒通道的所有事件
 * 然后将唤醒通道删除
 * 然后将唤醒描述符关闭
 * 最后将本线程的事件循环线程变量置为空
 */
EventLoop::~EventLoop()
{
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
            << " destructs in thread " << CurrentThread::tid();
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = NULL;
}

/**
 * 循环
 *
 * 首先将正在循环设置为真标记退出设置为假
 * 然后保持循环直到标记退出为真，循环中的操作如下：
 * 首先清空活跃通道列表
 * 然后调用轮询器的轮询方法，它会填充活跃通道列表，并且将轮询返回时间设置好
 * 然后将迭代次数加1
 * 然后将正在处理事件设置为真
 * 循环迭代活跃通道列表，对每一个活跃通道先将其设置为当前活跃通道，再调用其处理事件函数，在迭代完毕后将当前活跃通道设置为空
 * 然后将正在处理事件设置为假
 * 然后调用等待的函数列表中的所有函数
 *
 * 当退出循环时将正在循环设置为假
 */
void EventLoop::loop()
{
  assert(!looping_);
  assertInLoopThread();
  looping_ = true;
  quit_ = false;  // FIXME: what if someone calls quit() before loop() ?
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_)
  {
    activeChannels_.clear();
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
    ++iteration_;
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels();
    }
    // TODO sort channel by priority
    eventHandling_ = true;
    for (ChannelList::iterator it = activeChannels_.begin();
        it != activeChannels_.end(); ++it)
    {
      currentActiveChannel_ = *it;
      currentActiveChannel_->handleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = NULL;
    eventHandling_ = false;
    doPendingFunctors();
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

/**
 * 退出函数
 *
 * 将标记退出设置为真
 * 然后判断是否在循环线程，如果不是则唤醒循环线程
 */
void EventLoop::quit()
{
  quit_ = true;
  // There is a chance that loop() just executes while(!quit_) and exits,
  // then EventLoop destructs, then we are accessing an invalid object.
  // Can be fixed using mutex_ in both places.
  if (!isInLoopThread())
  {
    wakeup();
  }
}

/**
 * 在循环线程中执行
 *
 * 首先判断是否在循环线程
 * 如果是则直接调用回调
 * 否则将回调放到循环线程中排队
 */
void EventLoop::runInLoop(const Functor& cb)
{
  if (isInLoopThread())
  {
    cb();
  }
  else
  {
    queueInLoop(cb);
  }
}

/**
 * 在循环线程中排队
 *
 * 首先在互斥锁的保护下将回调加入正在等待的函数数组中
 * 然后判断是否不在循环线程或者正在调用等待的函数，如果是则唤醒循环线程
 */
void EventLoop::queueInLoop(const Functor& cb)
{
  {
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(cb);
  }

  if (!isInLoopThread() || callingPendingFunctors_)
  {
    wakeup();
  }
}

/**
 * 获得队列大小
 *
 * 在互斥锁的保护下返回正在等待的函数数组的大小
 */
size_t EventLoop::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return pendingFunctors_.size();
}

/**
 * 在指定时间点运行回调
 *
 * 调用时间队列的添加定时器函数在指定时间点调用回调
 */
TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb)
{
  return timerQueue_->addTimer(cb, time, 0.0);
}

/**
 * 在指定时间后运行回调
 *
 * 根据当前时间和指定的时间段计算指定的时间点
 * 然后在指定时间点运行回调
 */
TimerId EventLoop::runAfter(double delay, const TimerCallback& cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, cb);
}

/**
 * 每间隔时间运行回调
 *
 * 根据当前时间和时间间隔计算指定的时间点
 * 然后调用时间队列的添加定时器函数在指定时间点和时间间隔调用回调
 */
TimerId EventLoop::runEvery(double interval, const TimerCallback& cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(cb, time, interval);
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
// FIXME: remove duplication
void EventLoop::runInLoop(Functor&& cb)
{
  if (isInLoopThread())
  {
    cb();
  }
  else
  {
    queueInLoop(std::move(cb));
  }
}

void EventLoop::queueInLoop(Functor&& cb)
{
  {
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(std::move(cb));  // emplace_back
  }

  if (!isInLoopThread() || callingPendingFunctors_)
  {
    wakeup();
  }
}

TimerId EventLoop::runAt(const Timestamp& time, TimerCallback&& cb)
{
  return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback&& cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback&& cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(std::move(cb), time, interval);
}
#endif

/**
 * 取消迭代器
 *
 * 调用时间队列的取消迭代器函数
 */
void EventLoop::cancel(TimerId timerId)
{
  return timerQueue_->cancel(timerId);
}

/**
 * 更新通道
 *
 * 调用轮询器的更新通道函数
 */
void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

/**
 * 删除通道
 *
 * 调用轮询器的删除通道函数
 */
void EventLoop::removeChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

/**
 * 判断事件循环中是否有通道
 *
 * 调用轮询器的是否有通道函数
 */
bool EventLoop::hasChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  return poller_->hasChannel(channel);
}

/**
 * 因不在事件循环线程而退出
 */
void EventLoop::abortNotInLoopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

/**
 * 唤醒
 *
 * 对唤醒描述符写入8个字节
 */
void EventLoop::wakeup()
{
  uint64_t one = 1;
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

/**
 * 处理读
 *
 * 从唤醒描述符读出8个字节
 */
void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

/**
 * 调用正在等待的函数
 *
 * 将是否正在调用等待的函数设置为真
 * 在互斥锁的保护下将正在等待的函数数组中的元素取出并且清空
 * 分别调用取出的函数
 * 将是否正在调用等待的函数设置为假
 */
void EventLoop::doPendingFunctors()
{
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
  MutexLockGuard lock(mutex_);
  functors.swap(pendingFunctors_);
  }

  for (size_t i = 0; i < functors.size(); ++i)
  {
    functors[i]();
  }
  callingPendingFunctors_ = false;
}

/**
 * 打印活跃通道
 *
 * 循环遍历活跃通道列表，分别将每一个活跃通道接收到的事件转化为字符串输出到日志
 */
void EventLoop::printActiveChannels() const
{
  for (ChannelList::const_iterator it = activeChannels_.begin();
      it != activeChannels_.end(); ++it)
  {
    const Channel* ch = *it;
    LOG_TRACE << "{" << ch->reventsToString() << "} ";
  }
}

