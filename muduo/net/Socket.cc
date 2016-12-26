// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Socket.h>

#include <muduo/base/Logging.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <strings.h>  // bzero
#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

/**
 * 析构函数
 *
 * 关闭描述符
 */
Socket::~Socket()
{
  sockets::close(sockfd_);
}

/**
 * 获得tcp信息
 *
 * 首先清空tcpi指向的内存
 * 然后调用::getsockopt实现
 */
bool Socket::getTcpInfo(struct tcp_info* tcpi) const
{
  socklen_t len = sizeof(*tcpi);
  bzero(tcpi, len);
  return ::getsockopt(sockfd_, SOL_TCP, TCP_INFO, tcpi, &len) == 0;
}

/**
 * 获得tcp信息字符串
 *
 * 首先清空tcpi指向的内存
 * 然后调用::getsockopt
 * 最后将其转换为字符串
 */
bool Socket::getTcpInfoString(char* buf, int len) const
{
  struct tcp_info tcpi;
  bool ok = getTcpInfo(&tcpi);
  if (ok)
  {
    snprintf(buf, len, "unrecovered=%u "
             "rto=%u ato=%u snd_mss=%u rcv_mss=%u "
             "lost=%u retrans=%u rtt=%u rttvar=%u "
             "sshthresh=%u cwnd=%u total_retrans=%u",
             tcpi.tcpi_retransmits,  // Number of unrecovered [RTO] timeouts
             tcpi.tcpi_rto,          // Retransmit timeout in usec
             tcpi.tcpi_ato,          // Predicted tick of soft clock in usec
             tcpi.tcpi_snd_mss,
             tcpi.tcpi_rcv_mss,
             tcpi.tcpi_lost,         // Lost packets
             tcpi.tcpi_retrans,      // Retransmitted packets out
             tcpi.tcpi_rtt,          // Smoothed round trip time in usec
             tcpi.tcpi_rttvar,       // Medium deviation
             tcpi.tcpi_snd_ssthresh,
             tcpi.tcpi_snd_cwnd,
             tcpi.tcpi_total_retrans);  // Total retransmits for entire connection
  }
  return ok;
}

/**
 * 绑定地址
 *
 * 调用sockets::bindOrDie实现
 */
void Socket::bindAddress(const InetAddress& addr)
{
  sockets::bindOrDie(sockfd_, addr.getSockAddr());
}

/**
 * 监听
 *
 * 调用sockets::listenOrDie实现
 */
void Socket::listen()
{
  sockets::listenOrDie(sockfd_);
}

/**
 * 接受
 *
 * 调用sockets::accept实现
 */
int Socket::accept(InetAddress* peeraddr)
{
  struct sockaddr_in6 addr;
  bzero(&addr, sizeof addr);
  int connfd = sockets::accept(sockfd_, &addr);
  if (connfd >= 0)
  {
    peeraddr->setSockAddrInet6(addr);
  }
  return connfd;
}

/**
 * 关闭写
 *
 * 调用sockets::shutdownWrite实现
 */
void Socket::shutdownWrite()
{
  sockets::shutdownWrite(sockfd_);
}

/**
 * 设置tcp不延迟
 *
 * 调用::setsockopt实现
 */
void Socket::setTcpNoDelay(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
               &optval, static_cast<socklen_t>(sizeof optval));
  // FIXME CHECK
}

/**
 * 设置地址重用
 *
 * 调用::setsockopt实现
 */
void Socket::setReuseAddr(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
               &optval, static_cast<socklen_t>(sizeof optval));
  // FIXME CHECK
}

/**
 * 设置端口重用
 *
 * 调用::setsockopt实现实现
 */
void Socket::setReusePort(bool on)
{
#ifdef SO_REUSEPORT
  int optval = on ? 1 : 0;
  int ret = ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT,
                         &optval, static_cast<socklen_t>(sizeof optval));
  if (ret < 0 && on)
  {
    LOG_SYSERR << "SO_REUSEPORT failed.";
  }
#else
  if (on)
  {
    LOG_ERROR << "SO_REUSEPORT is not supported.";
  }
#endif
}

/**
 * 设置保活
 *
 * 调用::setsockopt实现
 */
void Socket::setKeepAlive(bool on)
{
  int optval = on ? 1 : 0;
  ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE,
               &optval, static_cast<socklen_t>(sizeof optval));
  // FIXME CHECK
}

