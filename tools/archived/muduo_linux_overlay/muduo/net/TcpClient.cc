// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//

// ---------------------------------------------------------------------------
// LOCAL OVERLAY -- 由 tools/archived/setup_dependencies.sh 在配置 muduo 之前
// 拷到 third_party/muduo-linux/muduo/net/TcpClient.cc。
//
// 除标 LOCAL DELTA 的两处外与上游逐字一致:构造函数里那条 FIXME setConnectFailedCallback
// 换成说明,新增 setConnectFailedCallback(把回调转存进 connector_)。动机见同目录
// Connector.cc 的说明。
//
// 升级 muduo 子模块时:用新的上游文件覆盖本文件,再逐处补回 LOCAL DELTA。
// ---------------------------------------------------------------------------

#include "muduo/net/TcpClient.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Connector.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/SocketsOps.h"

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

// TcpClient::TcpClient(EventLoop* loop)
//   : loop_(loop)
// {
// }

// TcpClient::TcpClient(EventLoop* loop, const string& host, uint16_t port)
//   : loop_(CHECK_NOTNULL(loop)),
//     serverAddr_(host, port)
// {
// }

namespace muduo
{
namespace net
{
namespace detail
{

void removeConnection(EventLoop* loop, const TcpConnectionPtr& conn)
{
  loop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}

void removeConnector(const ConnectorPtr& connector)
{
  //connector->
}

}  // namespace detail
}  // namespace net
}  // namespace muduo

TcpClient::TcpClient(EventLoop* loop,
                     const InetAddress& serverAddr,
                     const string& nameArg)
  : loop_(CHECK_NOTNULL(loop)),
    connector_(new Connector(loop, serverAddr)),
    name_(nameArg),
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    retry_(false),
    connect_(true),
    nextConnId_(1)
{
  connector_->setNewConnectionCallback(
      std::bind(&TcpClient::newConnection, this, _1));
  // LOCAL DELTA:上游这里的 FIXME setConnectFailedCallback 由 setConnectFailedCallback() 补上,
  // 回调直接存进 connector_(不经 TcpClient 转发、不捕获 this);不设时行为与上游相同。
  LOG_INFO << "TcpClient::TcpClient[" << name_
           << "] - connector " << get_pointer(connector_);
}

TcpClient::~TcpClient()
{
  LOG_INFO << "TcpClient::~TcpClient[" << name_
           << "] - connector " << get_pointer(connector_);
  TcpConnectionPtr conn;
  bool unique = false;
  {
    MutexLockGuard lock(mutex_);
    unique = connection_.unique();
    conn = connection_;
  }
  if (conn)
  {
    assert(loop_ == conn->getLoop());
    // FIXME: not 100% safe, if we are in different thread
    CloseCallback cb = std::bind(&detail::removeConnection, loop_, _1);
    loop_->runInLoop(
        std::bind(&TcpConnection::setCloseCallback, conn, cb));
    if (unique)
    {
      conn->forceClose();
    }
  }
  else
  {
    connector_->stop();
    // FIXME: HACK
    loop_->runAfter(1, std::bind(&detail::removeConnector, connector_));
  }
}

void TcpClient::connect()
{
  // FIXME: check state
  LOG_INFO << "TcpClient::connect[" << name_ << "] - connecting to "
           << connector_->serverAddress().toIpPort();
  connect_ = true;
  connector_->start();
}

void TcpClient::disconnect()
{
  connect_ = false;

  {
    MutexLockGuard lock(mutex_);
    if (connection_)
    {
      connection_->shutdown();
    }
  }
}

void TcpClient::stop()
{
  connect_ = false;
  connector_->stop();
}

// LOCAL DELTA:见 TcpClient.h。~TcpClient 没有连接时会 stop() Connector(connect_ 随即为 false),
// 之后 Connector 不再回调,所以回调不会在本对象析构后触发;使用方仍应只捕获 weak_ptr。
void TcpClient::setConnectFailedCallback(ConnectFailedCallback cb)
{
  connector_->setConnectFailedCallback(std::move(cb));
}

void TcpClient::newConnection(int sockfd)
{
  loop_->assertInLoopThread();
  InetAddress peerAddr(sockets::getPeerAddr(sockfd));
  char buf[32];
  snprintf(buf, sizeof buf, ":%s#%d", peerAddr.toIpPort().c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary
  TcpConnectionPtr conn(new TcpConnection(loop_,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));

  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
      std::bind(&TcpClient::removeConnection, this, _1)); // FIXME: unsafe
  {
    MutexLockGuard lock(mutex_);
    connection_ = conn;
  }
  conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  assert(loop_ == conn->getLoop());

  {
    MutexLockGuard lock(mutex_);
    assert(connection_ == conn);
    connection_.reset();
  }

  loop_->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
  if (retry_ && connect_)
  {
    LOG_INFO << "TcpClient::connect[" << name_ << "] - Reconnecting to "
             << connector_->serverAddress().toIpPort();
    connector_->restart();
  }
}

