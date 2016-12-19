// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_BUFFER_H
#define MUDUO_NET_BUFFER_H

#include <muduo/base/copyable.h>
#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>

#include <muduo/net/Endian.h>

#include <algorithm>
#include <vector>

#include <assert.h>
#include <string.h>
//#include <unistd.h>  // ssize_t

namespace muduo
{
namespace net
{

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode
/**
 * 缓冲类
 */
class Buffer : public muduo::copyable
{
 public:
  /**
   * 在前追加预留大小
   */
  static const size_t kCheapPrepend = 8;
  /**
   * 初始大小
   */
  static const size_t kInitialSize = 1024;

  /**
   * 构造函数
   */
  explicit Buffer(size_t initialSize = kInitialSize)
    : buffer_(kCheapPrepend + initialSize),
      readerIndex_(kCheapPrepend),
      writerIndex_(kCheapPrepend)
  {
    assert(readableBytes() == 0);
    assert(writableBytes() == initialSize);
    assert(prependableBytes() == kCheapPrepend);
  }

  // implicit copy-ctor, move-ctor, dtor and assignment are fine
  // NOTE: implicit move-ctor is added in g++ 4.6

  /**
   * 交换
   *
   * 交换内部的内存容器，读者索引和写者索引
   */
  void swap(Buffer& rhs)
  {
    buffer_.swap(rhs.buffer_);
    std::swap(readerIndex_, rhs.readerIndex_);
    std::swap(writerIndex_, rhs.writerIndex_);
  }

  /**
   * 可读字节数
   *
   * 返回写者索引减去读者索引
   */
  size_t readableBytes() const
  { return writerIndex_ - readerIndex_; }

  /**
   * 可写字节数
   *
   * 返回内存容器大小减去写者所以呢
   */
  size_t writableBytes() const
  { return buffer_.size() - writerIndex_; }

  /**
   * 在前可追加字节数
   *
   * 返回读者索引
   */
  size_t prependableBytes() const
  { return readerIndex_; }

  /**
   * 偷看
   *
   * 返回指向容器读者位置的指针
   */
  const char* peek() const
  { return begin() + readerIndex_; }

  /**
   * 找到换行
   *
   * 在容器可读区间中搜索换行符
   * 如果找不到返回空，能找到则返回指向该换行符的指针
   */
  const char* findCRLF() const
  {
    // FIXME: replace with memmem()?
    const char* crlf = std::search(peek(), beginWrite(), kCRLF, kCRLF+2);
    return crlf == beginWrite() ? NULL : crlf;
  }

  /**
   * 从指定开始位置找到换行
   *
   * 在容器从指定开始位置之后的可读区间中搜索换行符
   * 如果找不到返回空，能找到则返回指向该换行符的指针
   */
  const char* findCRLF(const char* start) const
  {
    assert(peek() <= start);
    assert(start <= beginWrite());
    // FIXME: replace with memmem()?
    const char* crlf = std::search(start, beginWrite(), kCRLF, kCRLF+2);
    return crlf == beginWrite() ? NULL : crlf;
  }

  /**
   * 找到换行符
   *
   * 在容器可读区间中搜索换行符
   * 如果找不到返回空，能找到则返回指向该换行符的指针
   */
  const char* findEOL() const
  {
    const void* eol = memchr(peek(), '\n', readableBytes());
    return static_cast<const char*>(eol);
  }

  /**
   * 从指定开始位置找到换行
   *
   * 在容器从指定开始位置之后的可读区间中搜索换行符
   * 如果找不到返回空，能找到则返回指向该换行符的指针
   */
  const char* findEOL(const char* start) const
  {
    assert(peek() <= start);
    assert(start <= beginWrite());
    const void* eol = memchr(start, '\n', beginWrite() - start);
    return static_cast<const char*>(eol);
  }

  // retrieve returns void, to prevent
  // string str(retrieve(readableBytes()), readableBytes());
  // the evaluation of two functions are unspecified
  /**
   * 消耗指定长度
   *
   * 首先判断指定长度是否小于可读字节，如果是则将读者索引增加指定长度
   * 否则调用retrieveAll消耗所有内容
   */
  void retrieve(size_t len)
  {
    assert(len <= readableBytes());
    if (len < readableBytes())
    {
      readerIndex_ += len;
    }
    else
    {
      retrieveAll();
    }
  }

  /**
   * 消耗指针前面的内容
   *
   * 首先求得指针前面有多少字节，然后消耗掉这些字节
   */
  void retrieveUntil(const char* end)
  {
    assert(peek() <= end);
    assert(end <= beginWrite());
    retrieve(end - peek());
  }

  /**
   * 消耗8字节
   */
  void retrieveInt64()
  {
    retrieve(sizeof(int64_t));
  }

  /**
   * 消耗4字节
   */
  void retrieveInt32()
  {
    retrieve(sizeof(int32_t));
  }

  /**
   * 消耗2字节
   */
  void retrieveInt16()
  {
    retrieve(sizeof(int16_t));
  }

  /**
   * 消耗1字节
   */
  void retrieveInt8()
  {
    retrieve(sizeof(int8_t));
  }

  /**
   * 消耗所有内容
   *
   * 直接将读者索引和写者索引置为在前追加预留大小
   */
  void retrieveAll()
  {
    readerIndex_ = kCheapPrepend;
    writerIndex_ = kCheapPrepend;
  }

  /**
   * 将所有内容拷贝到字符串中并消耗
   * 利用可读字节数调用retrieveAsString拷贝并消耗
   */
  string retrieveAllAsString()
  {
    return retrieveAsString(readableBytes());;
  }

  /**
   * 将指定长度的内容拷贝到字符串中并消耗
   *
   * 首先将指定长度的内容拷贝到字符串中并消耗
   * 最后返回拷贝出的字符串
   */
  string retrieveAsString(size_t len)
  {
    assert(len <= readableBytes());
    string result(peek(), len);
    retrieve(len);
    return result;
  }

  /**
   * 到字符串片
   *
   * 将可读字节封装成字符串片并返回
   */
  StringPiece toStringPiece() const
  {
    return StringPiece(peek(), static_cast<int>(readableBytes()));
  }

  /**
   * 追加字符串片的内容
   *
   * 取出字符串片的头指针和长度，调用追加重载
   */
  void append(const StringPiece& str)
  {
    append(str.data(), str.size());
  }

  /**
   * 追加指定长度的数据
   *
   * 将内存容器扩容至可写入指定长度
   * 然后从写者位置将指定长度的数据拷贝到内存容器
   * 最后将写者向后移动指定长度
   */
  void append(const char* /*restrict*/ data, size_t len)
  {
    ensureWritableBytes(len);
    std::copy(data, data+len, beginWrite());
    hasWritten(len);
  }

  /**
   * 追加指定长度的数据
   *
   * 将指针转型之后调用追加重载
   */
  void append(const void* /*restrict*/ data, size_t len)
  {
    append(static_cast<const char*>(data), len);
  }

  /**
   * 确保可写入指定长度
   *
   * 判断可写字节是否小于指定长度
   * 如果是则对容器进行扩容
   */
  void ensureWritableBytes(size_t len)
  {
    if (writableBytes() < len)
    {
      makeSpace(len);
    }
    assert(writableBytes() >= len);
  }

  /**
   * 返回指向写者位置的指针
   */
  char* beginWrite()
  { return begin() + writerIndex_; }

  /**
   * 返回指向写者位置的指针
   */
  const char* beginWrite() const
  { return begin() + writerIndex_; }

  /**
   * 已经写了指定长度
   *
   * 将写者位置向后移动指定长度
   */
  void hasWritten(size_t len)
  {
    assert(len <= writableBytes());
    writerIndex_ += len;
  }

  /**
   * 取消写指定长度
   *
   * 将写着位置向前移动指定长度
   */
  void unwrite(size_t len)
  {
    assert(len <= readableBytes());
    writerIndex_ -= len;
  }

  ///
  /// Append int64_t using network endian
  ///
  /**
   * 使用大端追加8字节整数
   *
   * 首先将小端整数转为大端整数，然后调用追加函数
   */
  void appendInt64(int64_t x)
  {
    int64_t be64 = sockets::hostToNetwork64(x);
    append(&be64, sizeof be64);
  }

  ///
  /// Append int32_t using network endian
  ///
  /**
   * 使用大端追加4字节整数
   *
   * 首先将小端整数转为大端整数，然后调用追加函数
   */
  void appendInt32(int32_t x)
  {
    int32_t be32 = sockets::hostToNetwork32(x);
    append(&be32, sizeof be32);
  }

  /**
   * 使用大端追加2字节整数
   *
   * 首先将小端整数转为大端整数，然后调用追加函数
   */
  void appendInt16(int16_t x)
  {
    int16_t be16 = sockets::hostToNetwork16(x);
    append(&be16, sizeof be16);
  }

  /**
   * 使用大端追加1字节整数
   *
   * 直接调用追加函数
   */
  void appendInt8(int8_t x)
  {
    append(&x, sizeof x);
  }

  ///
  /// Read int64_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  /**
   * 读取8字节整数
   *
   * 首先将8字节整数取出，然后将其消耗
   */
  int64_t readInt64()
  {
    int64_t result = peekInt64();
    retrieveInt64();
    return result;
  }

  ///
  /// Read int32_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  /**
   * 读取4字节整数
   *
   * 首先将4字节整数取出，然后将其消耗
   */
  int32_t readInt32()
  {
    int32_t result = peekInt32();
    retrieveInt32();
    return result;
  }

  /**
   * 读取2字节整数
   *
   * 首先将2字节整数取出，然后将其消耗
   */
  int16_t readInt16()
  {
    int16_t result = peekInt16();
    retrieveInt16();
    return result;
  }

  /**
   * 读取1字节整数
   *
   * 首先将1字节整数取出，然后将其消耗
   */
  int8_t readInt8()
  {
    int8_t result = peekInt8();
    retrieveInt8();
    return result;
  }

  ///
  /// Peek int64_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int64_t)
  /**
   * 取出8字节整数
   *
   * 首先从读者位置取出8个字节，然后将其从大端转换为小端
   */
  int64_t peekInt64() const
  {
    assert(readableBytes() >= sizeof(int64_t));
    int64_t be64 = 0;
    ::memcpy(&be64, peek(), sizeof be64);
    return sockets::networkToHost64(be64);
  }

  ///
  /// Peek int32_t from network endian
  ///
  /// Require: buf->readableBytes() >= sizeof(int32_t)
  /**
   * 取出4字节整数
   *
   * 首先从读者位置取出4个字节，然后将其从大端转换为小端
   *
   */
  int32_t peekInt32() const
  {
    assert(readableBytes() >= sizeof(int32_t));
    int32_t be32 = 0;
    ::memcpy(&be32, peek(), sizeof be32);
    return sockets::networkToHost32(be32);
  }

  /**
   * 取出2字节整数
   *
   * 首先从读者位置取出2个字节，然后将其从大端转换为小端
   *
   */
  int16_t peekInt16() const
  {
    assert(readableBytes() >= sizeof(int16_t));
    int16_t be16 = 0;
    ::memcpy(&be16, peek(), sizeof be16);
    return sockets::networkToHost16(be16);
  }

  /**
   * 取出1字节整数
   *
   * 从读者位置取出1个字节并返回
   *
   */
  int8_t peekInt8() const
  {
    assert(readableBytes() >= sizeof(int8_t));
    int8_t x = *peek();
    return x;
  }

  ///
  /// Prepend int64_t using network endian
  ///
  /**
   * 使用大端在前追加8字节整数
   *
   * 首先将整数从小端转为大端，然后调用在前追加函数
   */
  void prependInt64(int64_t x)
  {
    int64_t be64 = sockets::hostToNetwork64(x);
    prepend(&be64, sizeof be64);
  }

  ///
  /// Prepend int32_t using network endian
  ///
  /**
   * 使用大端在前追加4字节整数
   *
   * 首先将整数从小端转为大端，然后调用在前追加函数
   */
  void prependInt32(int32_t x)
  {
    int32_t be32 = sockets::hostToNetwork32(x);
    prepend(&be32, sizeof be32);
  }

  /**
   * 使用大端在前追加2字节整数
   *
   * 首先将整数从小端转为大端，然后调用在前追加函数
   */
  void prependInt16(int16_t x)
  {
    int16_t be16 = sockets::hostToNetwork16(x);
    prepend(&be16, sizeof be16);
  }

  /**
   * 使用大端在前追加1字节整数
   *
   * 直接调用在前追加函数
   */
  void prependInt8(int8_t x)
  {
    prepend(&x, sizeof x);
  }

  /**
   * 在前追加指定长度的数据
   *
   * 首先将读者位置向前移动指定长度
   * 然后将指定长度的数据拷贝到读者位置
   */
  void prepend(const void* /*restrict*/ data, size_t len)
  {
    assert(len <= prependableBytes());
    readerIndex_ -= len;
    const char* d = static_cast<const char*>(data);
    std::copy(d, d+len, begin()+readerIndex_);
  }

  /**
   * 收缩
   *
   * 首先创建一个新的缓冲器
   * 并且确保其可写入当前可读字节数加上保留字节数
   * 然后将当前缓冲器的可读内容拷贝到其中
   * 最后交换本缓冲器和新缓冲器
   */
  void shrink(size_t reserve)
  {
    // FIXME: use vector::shrink_to_fit() in C++ 11 if possible.
    Buffer other;
    other.ensureWritableBytes(readableBytes()+reserve);
    other.append(toStringPiece());
    swap(other);
  }

  /**
   * 内部容量
   *
   * 返回内存容器的容量
   */
  size_t internalCapacity() const
  {
    return buffer_.capacity();
  }

  /// Read data directly into buffer.
  ///
  /// It may implement with readv(2)
  /// @return result of read(2), @c errno is saved
  ssize_t readFd(int fd, int* savedErrno);

 private:

  /**
   * 返回指向容器起始位置的指针
   */
  char* begin()
  { return &*buffer_.begin(); }

  /**
   * 返回指向容器起始位置的指针
   */
  const char* begin() const
  { return &*buffer_.begin(); }

  /**
   * 制造指定长度的可写空间
   *
   * 如果可写长度加上可在前追加长度小于指定长度和加在前追加预留大小则将内容容器大小重置为写者位置加指定长度大小
   * 否则将可读区间拷贝到在前追加预留大小之后，并且重置读者和写者位置
   */
  void makeSpace(size_t len)
  {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend)
    {
      // FIXME: move readable data
      buffer_.resize(writerIndex_+len);
    }
    else
    {
      // move readable data to the front, make space inside buffer
      assert(kCheapPrepend < readerIndex_);
      size_t readable = readableBytes();
      std::copy(begin()+readerIndex_,
                begin()+writerIndex_,
                begin()+kCheapPrepend);
      readerIndex_ = kCheapPrepend;
      writerIndex_ = readerIndex_ + readable;
      assert(readable == readableBytes());
    }
  }

 private:
  /**
   * 内存容器
   */
  std::vector<char> buffer_;
  /**
   * 读者索引
   */
  size_t readerIndex_;
  /**
   * 写者索引
   */
  size_t writerIndex_;

  static const char kCRLF[];
};

}
}

#endif  // MUDUO_NET_BUFFER_H
