// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/InetAddress.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Endian.h>
#include <muduo/net/SocketsOps.h>

#include <netdb.h>
#include <strings.h>  // bzero
#include <netinet/in.h>

// INADDR_ANY use (type)value casting.
#pragma GCC diagnostic ignored "-Wold-style-cast"
/**
 * 定义任意地址
 */
static const in_addr_t kInaddrAny = INADDR_ANY;
/**
 * 定义回环地址
 */
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;
#pragma GCC diagnostic error "-Wold-style-cast"

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr_in6 {
//         sa_family_t     sin6_family;   /* address family: AF_INET6 */
//         uint16_t        sin6_port;     /* port in network byte order */
//         uint32_t        sin6_flowinfo; /* IPv6 flow information */
//         struct in6_addr sin6_addr;     /* IPv6 address */
//         uint32_t        sin6_scope_id; /* IPv6 scope-id */
//     };

using namespace muduo;
using namespace muduo::net;

static_assert(sizeof(InetAddress) == sizeof(struct sockaddr_in6),
              "InetAddress is same size as sockaddr_in6");
static_assert(offsetof(sockaddr_in, sin_family) == 0, "sin_family offset 0");
static_assert(offsetof(sockaddr_in6, sin6_family) == 0, "sin6_family offset 0");
static_assert(offsetof(sockaddr_in, sin_port) == 2, "sin_port offset 2");
static_assert(offsetof(sockaddr_in6, sin6_port) == 2, "sin6_port offset 2");

#if !(__GNUC_PREREQ (4,6))
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
/**
 * 构造函数
 *
 * 首先判断是否是v6如果是则初始化v6地址否则初始化普通地址
 * 初始化过程中首先初始化协议族
 * 然后判断只绑定回环地址，如果是则设置为回环地址，否则设置为任意地址
 * 然后将端口编码为大端后设置好
 */
InetAddress::InetAddress(uint16_t port, bool loopbackOnly, bool ipv6)
{
  static_assert(offsetof(InetAddress, addr6_) == 0, "addr6_ offset 0");
  static_assert(offsetof(InetAddress, addr_) == 0, "addr_ offset 0");
  if (ipv6)
  {
    bzero(&addr6_, sizeof addr6_);
    addr6_.sin6_family = AF_INET6;
    in6_addr ip = loopbackOnly ? in6addr_loopback : in6addr_any;
    addr6_.sin6_addr = ip;
    addr6_.sin6_port = sockets::hostToNetwork16(port);
  }
  else
  {
    bzero(&addr_, sizeof addr_);
    addr_.sin_family = AF_INET;
    in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
    addr_.sin_addr.s_addr = sockets::hostToNetwork32(ip);
    addr_.sin_port = sockets::hostToNetwork16(port);
  }
}

/**
 * 构造函数
 *
 * 首先判断是否是v6，如果是则初始化v6地址，否则初始化普通地址
 * 初始化过程调用sockets::fromIpPort实现
 */
InetAddress::InetAddress(StringArg ip, uint16_t port, bool ipv6)
{
  if (ipv6)
  {
    bzero(&addr6_, sizeof addr6_);
    sockets::fromIpPort(ip.c_str(), port, &addr6_);
  }
  else
  {
    bzero(&addr_, sizeof addr_);
    sockets::fromIpPort(ip.c_str(), port, &addr_);
  }
}

/**
 * 将网络地址转换为地址和端口的字符串
 *
 * 首先获得网络地址然后调用sockets::toIpPort实现
 */
string InetAddress::toIpPort() const
{
  char buf[64] = "";
  sockets::toIpPort(buf, sizeof buf, getSockAddr());
  return buf;
}

/**
 * 将网络地址转换为地址字符串
 *
 * 首先获得网络地址然后调用sockets::toIp实现
 */
string InetAddress::toIp() const
{
  char buf[64] = "";
  sockets::toIp(buf, sizeof buf, getSockAddr());
  return buf;
}

/**
 * 返回大端网络地址
 */
uint32_t InetAddress::ipNetEndian() const
{
  assert(family() == AF_INET);
  return addr_.sin_addr.s_addr;
}

/**
 * 将网络地址转换为端口号
 *
 * 首先获得大端端口号然后将其转换为小端
 */
uint16_t InetAddress::toPort() const
{
  return sockets::networkToHost16(portNetEndian());
}

/**
 * 解析缓冲区
 */
static __thread char t_resolveBuffer[64 * 1024];

/**
 * 将域名转换为网络地址
 *
 * 调用gethostbyname_r实现
 */
bool InetAddress::resolve(StringArg hostname, InetAddress* out)
{
  assert(out != NULL);
  struct hostent hent;
  struct hostent* he = NULL;
  int herrno = 0;
  bzero(&hent, sizeof(hent));

  int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer, sizeof t_resolveBuffer, &he, &herrno);
  if (ret == 0 && he != NULL)
  {
    assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
    out->addr_.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
    return true;
  }
  else
  {
    if (ret)
    {
      LOG_SYSERR << "InetAddress::resolve";
    }
    return false;
  }
}
