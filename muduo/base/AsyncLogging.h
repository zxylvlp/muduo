#ifndef MUDUO_BASE_ASYNCLOGGING_H
#define MUDUO_BASE_ASYNCLOGGING_H

#include <muduo/base/BlockingQueue.h>
#include <muduo/base/BoundedBlockingQueue.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>

#include <muduo/base/LogStream.h>

namespace muduo
{

class AsyncLogging : noncopyable
{
 public:

  AsyncLogging(const string& basename,
               size_t rollSize,
               int flushInterval = 3);

  /**
   * 析构函数
   *
   * 如果正在运行则调用停止函数
   */
  ~AsyncLogging()
  {
    if (running_)
    {
      stop();
    }
  }

  void append(const char* logline, int len);

  /**
   * 开始
   *
   * 将正在运行设置为真，开启线程并等待在锁存器上
   */
  void start()
  {
    running_ = true;
    thread_.start();
    latch_.wait();
  }

  /**
   * 停止
   *
   * 将正在运行设置为假，唤醒条件变量并且等待线程结束
   */
  void stop()
  {
    running_ = false;
    cond_.notify();
    thread_.join();
  }

 private:

  void threadFunc();

  /**
   * 定义缓冲类型为固定缓冲
   */
  typedef muduo::detail::FixedBuffer<muduo::detail::kLargeBuffer> Buffer;
  /**
   * 定义缓冲数组类型为缓冲的唯一指针组成的数组
   */
  typedef std::vector<std::unique_ptr<Buffer>> BufferVector;
  /**
   * 定义缓冲数组的值类型为缓冲指针
   */
  typedef BufferVector::value_type BufferPtr;

  /**
   * 刷盘间隔时间
   */
  const int flushInterval_;
  /**
   * 是否正在运行
   */
  bool running_;
  /**
   * 基本名称
   */
  string basename_;
  /**
   * 回滚大小
   */
  size_t rollSize_;
  /**
   * 线程
   */
  muduo::Thread thread_;
  /**
   * 锁存器
   */
  muduo::CountDownLatch latch_;
  /**
   * 互斥锁
   */
  muduo::MutexLock mutex_;
  /**
   * 条件变量
   */
  muduo::Condition cond_;
  /**
   * 指向当前缓冲的指针
   */
  BufferPtr currentBuffer_;
  /**
   * 指向下一个缓冲的指针
   */
  BufferPtr nextBuffer_;
  /**
   * 缓冲数组
   */
  BufferVector buffers_;
};

}
#endif  // MUDUO_BASE_ASYNCLOGGING_H
