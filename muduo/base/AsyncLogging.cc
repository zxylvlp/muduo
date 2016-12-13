#include <muduo/base/AsyncLogging.h>
#include <muduo/base/LogFile.h>
#include <muduo/base/Timestamp.h>

#include <stdio.h>

using namespace muduo;

/**
 * 构造函数
 *
 * 首先初始化成员
 * 然后将当前缓冲、下一个缓冲清零
 * 最后将缓冲数组的容量设置为16
 */
AsyncLogging::AsyncLogging(const string& basename,
                           size_t rollSize,
                           int flushInterval)
  : flushInterval_(flushInterval),
    running_(false),
    basename_(basename),
    rollSize_(rollSize),
    thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
    latch_(1),
    mutex_(),
    cond_(mutex_),
    currentBuffer_(new Buffer),
    nextBuffer_(new Buffer),
    buffers_()
{
  currentBuffer_->bzero();
  nextBuffer_->bzero();
  buffers_.reserve(16);
}

/**
 * 追加数据
 *
 * 在互斥锁保护下完成以下操作
 * 首先判断当期那缓冲能否装下这条日志，如果可以则直接追加到当前缓冲并返回
 * 否则将当前缓冲添加到缓冲数组中，如果下一个缓冲存在则将下一个缓冲作为当前缓冲并清空下一个缓冲，否则直接创建一个缓冲作为当前缓冲
 * 然后将日志追加到当前缓冲并且唤醒等待在条件变量上的线程
 */
void AsyncLogging::append(const char* logline, int len)
{
  muduo::MutexLockGuard lock(mutex_);
  if (currentBuffer_->avail() > len)
  {
    currentBuffer_->append(logline, len);
  }
  else
  {
    buffers_.push_back(std::move(currentBuffer_));

    if (nextBuffer_)
    {
      currentBuffer_ = std::move(nextBuffer_);
    }
    else
    {
      currentBuffer_.reset(new Buffer); // Rarely happens
    }
    currentBuffer_->append(logline, len);
    cond_.notify();
  }
}

/**
 * 线程函数
 *
 * 首先将锁存器减一
 * 然后创建输出日志文件对象
 * 然后创建两个新缓冲并清空
 * 然后创建一个局部缓冲数组
 * 如果正在运行则持续进行以下循环：
 * 加互斥锁，如果缓冲数组是空，则在条件变量上最多等待几秒，将当前缓冲移动到缓冲数组中，将第一个新缓冲移动到当前缓冲中，
 * 交换局部缓冲数组和缓冲数组的内容，如果下一个缓冲为空则将第二个新缓冲移动到其中
 * 如果局部缓冲数组过大则将其长度设置为2
 * 将局部缓冲数组中的内容写入日志
 * 如果局部缓冲数组的长度大于2则将其重置为2
 * 如果第一个新缓冲为空则将局部缓冲数组的最后一个移动给他并且清空
 * 如果第二个新缓冲为空则将局部缓冲数组的最后一个移动给他并且清空
 * 清空局部缓冲数组，将日志刷到内核
 *
 * 将日志刷到内核
 */
void AsyncLogging::threadFunc()
{
  assert(running_ == true);
  latch_.countDown();
  LogFile output(basename_, rollSize_, false);
  BufferPtr newBuffer1(new Buffer);
  BufferPtr newBuffer2(new Buffer);
  newBuffer1->bzero();
  newBuffer2->bzero();
  BufferVector buffersToWrite;
  buffersToWrite.reserve(16);
  while (running_)
  {
    assert(newBuffer1 && newBuffer1->length() == 0);
    assert(newBuffer2 && newBuffer2->length() == 0);
    assert(buffersToWrite.empty());

    {
      muduo::MutexLockGuard lock(mutex_);
      if (buffers_.empty())  // unusual usage!
      {
        cond_.waitForSeconds(flushInterval_);
      }
      buffers_.push_back(std::move(currentBuffer_));
      currentBuffer_ = std::move(newBuffer1);
      buffersToWrite.swap(buffers_);
      if (!nextBuffer_)
      {
        nextBuffer_ = std::move(newBuffer2);
      }
    }

    assert(!buffersToWrite.empty());

    if (buffersToWrite.size() > 25)
    {
      char buf[256];
      snprintf(buf, sizeof buf, "Dropped log messages at %s, %zd larger buffers\n",
               Timestamp::now().toFormattedString().c_str(),
               buffersToWrite.size()-2);
      fputs(buf, stderr);
      output.append(buf, static_cast<int>(strlen(buf)));
      buffersToWrite.erase(buffersToWrite.begin()+2, buffersToWrite.end());
    }

    for (size_t i = 0; i < buffersToWrite.size(); ++i)
    {
      // FIXME: use unbuffered stdio FILE ? or use ::writev ?
      output.append(buffersToWrite[i]->data(), buffersToWrite[i]->length());
    }

    if (buffersToWrite.size() > 2)
    {
      // drop non-bzero-ed buffers, avoid trashing
      buffersToWrite.resize(2);
    }

    if (!newBuffer1)
    {
      assert(!buffersToWrite.empty());
      newBuffer1 = std::move(buffersToWrite.back());
      buffersToWrite.back();
      newBuffer1->reset();
    }

    if (!newBuffer2)
    {
      assert(!buffersToWrite.empty());
      newBuffer2 = std::move(buffersToWrite.back());
      buffersToWrite.back();
      newBuffer2->reset();
    }

    buffersToWrite.clear();
    output.flush();
  }
  output.flush();
}

