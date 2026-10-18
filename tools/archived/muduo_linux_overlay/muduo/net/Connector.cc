// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

// ---------------------------------------------------------------------------
// LOCAL OVERLAY -- 由 tools/archived/setup_dependencies.sh 在配置 muduo 之前
// 拷到 third_party/muduo-linux/muduo/net/Connector.cc。
//
// 除标 LOCAL DELTA 的几处外与上游逐字一致,改动两件事:
//   1. 连接失败回调:retry() 排下一次重试之前、connect() 的致命错误分支,都回调
//      connectFailedCallback_(上游 TcpClient.cc 标了 FIXME setConnectFailedCallback,
//      本文件里的 connectErrorCallback_() 也被注释掉)。使用方是 muduo_grpc::Http2Transport:
//      被拒的连接立即进 TRANSIENT_FAILURE,而不是干等 connect_timeout
//      (docs/design/grpc-muduo-transport.md §5)。
//   2. 去掉上游绑裸 this 的 FIXME: unsafe(AGENTS.md §11.7):start() 的 startInLoop、stop() 的
//      stopInLoop、removeAndResetChannel() 排队的 resetChannel 改持 shared_from_this();connecting()
//      绑进自有 channel_ 的 write / error 回调属于「同寿命成员」例外,保留裸 this,只把 FIXME 换成理由。
// Windows 的两份树(cpp/libs/engine/muduo_windows/src、third_party/muduo)带同样的改动。
//
// 放在这里而不改子模块,理由同本目录的 TimerQueue.cc:子模块只提交指针,改它的工作区
// 在新克隆里不会保留;delta 留在本仓,上游检出保持原样。
//
// 升级 muduo 子模块时:用新的上游文件覆盖本文件,再逐处补回 LOCAL DELTA。
// ---------------------------------------------------------------------------

#include "muduo/net/Connector.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/SocketsOps.h"

#include <errno.h>

using namespace muduo;
using namespace muduo::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
  : loop_(loop),
    serverAddr_(serverAddr),
    connect_(false),
    state_(kDisconnected),
    retryDelayMs_(kInitRetryDelayMs)
{
  LOG_DEBUG << "ctor[" << this << "]";
}

Connector::~Connector()
{
  LOG_DEBUG << "dtor[" << this << "]";
  assert(!channel_);
}

void Connector::start()
{
  connect_ = true;
  // LOCAL DELTA(上游绑裸 this,标了 FIXME: unsafe):改绑 shared_from_this()(AGENTS.md §11.7)。
  // 不在 loop 线程调用时 startInLoop 排到队尾才执行,裸 this 届时可能已悬空;在 loop 线程调用时,
  // runInLoop 按值持有的函数对象也让 Connector 活过 startInLoop → connect() 整个栈(其中可能回调连接失败)。
  // 构造期间不会走到这里:Connector 只由 TcpClient 以 shared_ptr 持有,start() 只经 TcpClient::connect() 调用。
  loop_->runInLoop(std::bind(&Connector::startInLoop, shared_from_this()));
}

void Connector::startInLoop()
{
  loop_->assertInLoopThread();
  assert(state_ == kDisconnected);
  if (connect_)
  {
    connect();
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

void Connector::stop()
{
  connect_ = false;
  // LOCAL DELTA(上游绑裸 this,标了 FIXME: unsafe):改绑 shared_from_this()(AGENTS.md §11.7)。
  // stopInLoop 排到下一轮才执行;TcpClient 有连接时析构不给 Connector 续命,若此前调过 stop(),
  // 裸 this 到那时可能已悬空。持有 shared_ptr 后,任何时机调用 stop() 都安全。
  loop_->queueInLoop(std::bind(&Connector::stopInLoop, shared_from_this()));
  // FIXME: cancel timer
}

void Connector::stopInLoop()
{
  loop_->assertInLoopThread();
  if (state_ == kConnecting)
  {
    setState(kDisconnected);
    int sockfd = removeAndResetChannel();
    retry(sockfd, 0);
  }
}

void Connector::connect()
{
  int sockfd = sockets::createNonblockingOrDie(serverAddr_.family());
  int ret = sockets::connect(sockfd, serverAddr_.getSockAddr());
  int savedErrno = (ret == 0) ? 0 : errno;
  switch (savedErrno)
  {
    case 0:
    case EINPROGRESS:
    case EINTR:
    case EISCONN:
      connecting(sockfd);
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(sockfd, savedErrno);
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);
      // LOCAL DELTA:致命错误照旧不重试,但要通知使用方,否则它只能干等自己的超时。
      notifyConnectFailed(savedErrno);
      break;

    default:
      LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);
      // LOCAL DELTA:接上上游注释掉的 connectErrorCallback_(),理由同上。
      notifyConnectFailed(savedErrno);
      break;
  }
}

void Connector::restart()
{
  loop_->assertInLoopThread();
  setState(kDisconnected);
  retryDelayMs_ = kInitRetryDelayMs;
  connect_ = true;
  startInLoop();
}

void Connector::connecting(int sockfd)
{
  setState(kConnecting);
  assert(!channel_);
  channel_.reset(new Channel(loop_, sockfd));
  // LOCAL DELTA(只去掉上游这两行的 FIXME: unsafe,绑定照旧是裸 this):属于 AGENTS.md §11.7
  // 「与被绑对象同寿命的成员」例外。回调存在 channel_ 里,channel_ 由本对象独占,只由 Channel::handleEvent
  // 在 channel_ 登记于 Poller 期间调用;登记只在 removeAndResetChannel 里摘除,摘除后不会再有新的派发
  // (同一次 handleEvent 余下的回调仍会执行:连接失败常是 error 与 write 同报,handleError 摘除后
  // handleWrite 看到 kDisconnected 什么也不做),而摘除时排队的 resetChannel 持有 shared_from_this(),
  // 所以 channel_ 能派发回调时(含上述余下的回调)本对象必然还活着。
  // 登记期间(kConnecting)TcpClient 没有连接(TcpClient 正常用法),~TcpClient 会走 stop(),排队的 stopInLoop
  // 同样持有 shared_ptr,摘除前本对象不会析构。
  channel_->setWriteCallback(
      std::bind(&Connector::handleWrite, this));
  channel_->setErrorCallback(
      std::bind(&Connector::handleError, this));

  // channel_->tie(shared_from_this()); is not working,
  // as channel_ is not managed by shared_ptr
  channel_->enableWriting();
}

int Connector::removeAndResetChannel()
{
  channel_->disableAll();
  channel_->remove();
  int sockfd = channel_->fd();
  // Can't reset channel_ here, because we are inside Channel::handleEvent
  // LOCAL DELTA(上游绑裸 this,标了 FIXME: unsafe):改绑 shared_from_this()(AGENTS.md §11.7)。
  // resetChannel 排到下一轮才执行;刚连上时 ~TcpClient(有连接)不给 Connector 续命,使用方若在此之前
  // 析构 TcpClient,Connector 会带着未释放的 channel_ 析构(assert(!channel_);此刻可能还在 channel_ 的
  // handleEvent 栈上),排队的裸 this 随后悬空。持有 shared_ptr 后,本对象至少活到 channel_ 释放。
  // 调用方 stopInLoop / handleWrite / handleError 都要求 state_ == kConnecting,构造期间到达不了。
  loop_->queueInLoop(std::bind(&Connector::resetChannel, shared_from_this()));
  return sockfd;
}

void Connector::resetChannel()
{
  channel_.reset();
}

void Connector::handleWrite()
{
  LOG_TRACE << "Connector::handleWrite " << state_;

  if (state_ == kConnecting)
  {
    int sockfd = removeAndResetChannel();
    int err = sockets::getSocketError(sockfd);
    if (err)
    {
      LOG_WARN << "Connector::handleWrite - SO_ERROR = "
               << err << " " << strerror_tl(err);
      retry(sockfd, err);
    }
    else if (sockets::isSelfConnect(sockfd))
    {
      LOG_WARN << "Connector::handleWrite - Self connect";
      retry(sockfd, 0);
    }
    else
    {
      setState(kConnected);
      if (connect_)
      {
        newConnectionCallback_(sockfd);
      }
      else
      {
        sockets::close(sockfd);
      }
    }
  }
  else
  {
    // what happened?
    assert(state_ == kDisconnected);
  }
}

void Connector::handleError()
{
  LOG_ERROR << "Connector::handleError state=" << state_;
  if (state_ == kConnecting)
  {
    int sockfd = removeAndResetChannel();
    int err = sockets::getSocketError(sockfd);
    LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
    retry(sockfd, err);
  }
}

void Connector::retry(int sockfd, int savedErrno)
{
  sockets::close(sockfd);
  setState(kDisconnected);
  // LOCAL DELTA:排下一次重试之前通知连接失败。回调可能 stop(),所以下面重新读 connect_;
  // stop() 排队的 stopInLoop 届时看到 kDisconnected,什么也不做。
  notifyConnectFailed(savedErrno);
  if (connect_)
  {
    LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
             << " in " << retryDelayMs_ << " milliseconds. ";
    loop_->runAfter(retryDelayMs_/1000.0,
                    std::bind(&Connector::startInLoop, shared_from_this()));
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

// LOCAL DELTA:已 stop() 的不再通知(stopInLoop 也经 retry 关 socket,那不是连接失败)。
void Connector::notifyConnectFailed(int savedErrno)
{
  if (connect_ && connectFailedCallback_)
  {
    connectFailedCallback_(savedErrno);
  }
}

