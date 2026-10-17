// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/EventLoop.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/Channel.h"
#include "muduo/net/Poller.h"
#include "muduo/net/SocketsOps.h"
#include "muduo/net/TimerQueue.h"

#include <algorithm>

#include <signal.h>
#include <sys/eventfd.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{
__thread EventLoop* t_loopInThisThread = 0;

#ifdef __linux__
const int kPollTimeMs = 10000;
#endif // __linux__

#ifdef WIN32
const int kPollTimeMs = 100;
#endif // WIN32

#ifdef WIN32
// Windows 没有 eventfd,唤醒 fd 用一个连到自己的回环 UDP socket:wakeup() 往里 send 1 字节,
// WSAPoll 随即报可读,阻塞中的 poll 立刻返回;handleRead() 把积压的字节读干净。
// 只占一个 socket、存进现有的 wakeupFd_,EventLoop.h 的布局不变(使用方 include 的是
// third_party/muduo 那份头,muduo.lib 用本树的头编,数据成员一变就是静默的 ABI 错配)。
// connect 到自己之后,系统只收来自本端口的数据报,别的进程发到这个端口的包会被丢掉,不能把 loop 打成空转。
// 任何一步失败都直接 LOG_SYSFATAL,与 Linux 建 eventfd 失败的处理一致;Winsock 的错误码不进 errno,单独打出来。
int createEventfd()
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

  // 取回系统分配的端口,再把对端设成自己。
  int addrLen = static_cast<int>(sizeof addr);
  if (::getsockname(sock, reinterpret_cast<struct sockaddr*>(&addr), &addrLen) == SOCKET_ERROR)
  {
    LOG_SYSFATAL << "Failed in createEventfd: getsockname, WSAGetLastError=" << ::WSAGetLastError();
  }
  if (::connect(sock, reinterpret_cast<const struct sockaddr*>(&addr), static_cast<int>(sizeof addr)) == SOCKET_ERROR)
  {
    LOG_SYSFATAL << "Failed in createEventfd: connect, WSAGetLastError=" << ::WSAGetLastError();
  }

  // 非阻塞:wakeup() 在接收缓冲满时不能卡住投递线程,handleRead() 要靠 WSAEWOULDBLOCK 判断已读干净。
  u_long nonBlocking = 1;
  if (::ioctlsocket(sock, FIONBIO, &nonBlocking) == SOCKET_ERROR)
  {
    LOG_SYSFATAL << "Failed in createEventfd: ioctlsocket, WSAGetLastError=" << ::WSAGetLastError();
  }
  // 与本树其余 socket 一样按 int 存(见 sockets::createNonblockingOrDie)。
  return static_cast<int>(sock);
}
#else
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
#endif // WIN32

#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
 public:
  IgnoreSigPipe()
  {
#ifdef __linux__
    ::signal(SIGPIPE, SIG_IGN);
#endif // __linux__
    // LOG_TRACE << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;
}  // namespace

EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

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

EventLoop::~EventLoop()
{
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
            << " destructs in thread " << CurrentThread::tid();
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
#ifdef __linux
  ::close(wakeupFd_);
#endif // __linux
#ifdef WIN32
  ::winclosesock(wakeupFd_);
#endif // WIN32
  t_loopInThisThread = NULL;
}

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
#ifdef WIN32
    // No timerfd here, so the poller holds nothing that becomes readable when
    // a timer comes due -- timerQueue_->loop() below is what actually sweeps
    // the queue, and it only runs once per poll. Blocking for a constant
    // kPollTimeMs therefore rounds every timer up to that period. Derive the
    // timeout from the queue instead.
    pollReturnTime_ = poller_->poll(timerQueue_->nextTimeoutMs(kPollTimeMs), &activeChannels_);
#else
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
#endif // WIN32
    ++iteration_;
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels();
    }
    // TODO sort channel by priority
    eventHandling_ = true;
    for (Channel* channel : activeChannels_)
    {
      currentActiveChannel_ = channel;
      currentActiveChannel_->handleEvent(pollReturnTime_);
    }
#ifdef WIN32
	timerQueue_->loop();
#endif // WIN32
    currentActiveChannel_ = NULL;
    eventHandling_ = false;
    doPendingFunctors();
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

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

void EventLoop::runInLoop(Functor cb)
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

void EventLoop::queueInLoop(Functor cb)
{
  {
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(std::move(cb));
  }

  if (!isInLoopThread() || callingPendingFunctors_)
  {
    wakeup();
  }
}

size_t EventLoop::queueSize() const
{
  MutexLockGuard lock(mutex_);
  return pendingFunctors_.size();
}

TimerId EventLoop::runAt(Timestamp time, TimerCallback cb)
{
  return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancel(TimerId timerId)
{
  return timerQueue_->cancel(timerId);
}

void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

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

bool EventLoop::hasChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  return poller_->hasChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

void EventLoop::wakeup()
{
#ifdef __linux__
  uint64_t one = 1;
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
#endif // __linux__
#ifdef WIN32
  // 每次唤醒是一个 1 字节数据报(createEventfd 的回环 UDP socket),可从任意线程调用。
  // EWOULDBLOCK 只在缓冲已满时出现(接收缓冲满时系统也可能静默丢包):那时已有没读走的唤醒在排队,
  // poll 本来就会返回,这一次丢掉无妨。
  const char one = 1;
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    const int savedErrno = n < 0 ? errno : 0;
    if (savedErrno != EWOULDBLOCK)
    {
      LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 1, errno=" << savedErrno;
    }
  }
#endif // WIN32
}

void EventLoop::handleRead()
{
#ifdef __linux__
  uint64_t one = 1;
  ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
#endif // __linux__
#ifdef WIN32
  // WSAPoll 是水平触发:积压的唤醒数据报不读干净,下一次 poll 立刻又返回,loop 空转。
  // 一次 recv 只取走一个数据报,所以一直读到 EWOULDBLOCK。唤醒只需要「至少一次」,读掉多少个都不影响语义。
  char buf[64];
  for (;;)
  {
    ssize_t n = sockets::read(wakeupFd_, buf, sizeof buf);
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
#endif // WIN32
}

void EventLoop::doPendingFunctors()
{
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
  MutexLockGuard lock(mutex_);
  functors.swap(pendingFunctors_);
  }

  for (const Functor& functor : functors)
  {
    functor();
  }
  callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
  for (const Channel* channel : activeChannels_)
  {
    LOG_TRACE << "{" << channel->reventsToString() << "} ";
  }
}

