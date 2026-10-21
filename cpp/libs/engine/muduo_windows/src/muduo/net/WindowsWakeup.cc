// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

#include "muduo/net/WindowsWakeup.h"

#ifdef WIN32

#include "muduo/base/Logging.h"
#include "muduo/base/Types.h"
#include "muduo/net/SocketsOps.h"

#include <errno.h>

namespace muduo
{
namespace net
{
namespace windows
{

// Windows 没有 eventfd，单个自连回环 UDP socket 让 WSAPoll 能被跨线程投递立即唤醒。
// connect 到自己之后，系统只收来自本端口的数据报，其他进程不能把 loop 打成空转。
// 任何一步失败都直接 LOG_SYSFATAL，与 Linux 创建 eventfd 失败的处理一致。
int createWakeupFd()
{
  // WSA_FLAG_NO_HANDLE_INHERIT 对应 Linux 的 EFD_CLOEXEC。
  const SOCKET sock = ::WSASocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT);
  if (sock == INVALID_SOCKET)
  {
    LOG_SYSFATAL << "Failed in createEventfd: socket, WSAGetLastError=" << ::WSAGetLastError();
  }

  struct sockaddr_in addr;
  memZero(&addr, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (::bind(sock, reinterpret_cast<const struct sockaddr*>(&addr), static_cast<int>(sizeof addr)) == SOCKET_ERROR)
  {
    LOG_SYSFATAL << "Failed in createEventfd: bind, WSAGetLastError=" << ::WSAGetLastError();
  }

  // 取回系统分配的端口，再把对端设成自己。
  int addrLen = static_cast<int>(sizeof addr);
  if (::getsockname(sock, reinterpret_cast<struct sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
  {
    LOG_SYSFATAL << "Failed in createEventfd: getsockname, WSAGetLastError=" << ::WSAGetLastError();
  }
  if (::connect(sock, reinterpret_cast<const struct sockaddr*>(&addr), static_cast<int>(sizeof addr)) == SOCKET_ERROR)
  {
    LOG_SYSFATAL << "Failed in createEventfd: connect, WSAGetLastError=" << ::WSAGetLastError();
  }

  // 唤醒不能阻塞投递线程，排空则靠 WSAEWOULDBLOCK 判断结束。
  u_long nonBlocking = 1;
  if (::ioctlsocket(sock, FIONBIO, &nonBlocking) == SOCKET_ERROR)
  {
    LOG_SYSFATAL << "Failed in createEventfd: ioctlsocket, WSAGetLastError=" << ::WSAGetLastError();
  }
  // 与本树其余 socket 一样按 int 存（见 sockets::createNonblockingOrDie）。
  return static_cast<int>(sock);
}

void wakeup(int wakeupFd)
{
  // 接收缓冲满时系统也可能静默丢包；已有通知会让 poll 返回，允许合并本次唤醒。
  const char one = 1;
  ssize_t n = sockets::write(wakeupFd, &one, sizeof one);
  if (n != sizeof one)
  {
    const int savedErrno = n < 0 ? errno : 0;
    if (savedErrno != EWOULDBLOCK)
    {
      LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 1, errno=" << savedErrno;
    }
  }
}

void drainWakeupFd(int wakeupFd)
{
  // WSAPoll 是水平触发，一次 recv 只取走一个数据报，必须读到 EWOULDBLOCK 才不会空转。
  // 唤醒只需要「至少一次」，读掉多少个数据报都不影响已入队的任务。
  char buf[64];
  for (;;)
  {
    ssize_t n = sockets::read(wakeupFd, buf, sizeof buf);
    if (n >= 0)
    {
      continue;
    }
    const int savedErrno = errno;
    if (savedErrno != EWOULDBLOCK)
    {
      LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes, errno=" << savedErrno;
    }
    break;
  }
}

}
}
}

#endif
