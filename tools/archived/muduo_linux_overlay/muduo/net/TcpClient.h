// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

// ---------------------------------------------------------------------------
// LOCAL OVERLAY -- 由 tools/archived/setup_dependencies.sh 在配置 muduo 之前
// 拷到 third_party/muduo-linux/muduo/net/TcpClient.h。
//
// 除标 LOCAL DELTA 的一处外与上游逐字一致:新增 setConnectFailedCallback(只加成员函数,
// 不加数据成员,TcpClient 被 rpc_client.h、http2_transport.h 按值嵌入)。动机见同目录
// Connector.cc 的说明。游戏代码 include 的是应用 overlay 之后的本头,所以必须先跑
// setup_dependencies.sh(Docker 的 deps 阶段已包含),否则编到这个新方法时直接报错。
//
// 升级 muduo 子模块时:用新的上游文件覆盖本文件,再逐处补回 LOCAL DELTA。
// ---------------------------------------------------------------------------

#ifndef MUDUO_NET_TCPCLIENT_H
#define MUDUO_NET_TCPCLIENT_H

#include "muduo/base/Mutex.h"
#include "muduo/net/TcpConnection.h"

namespace muduo
{
namespace net
{

class Connector;
typedef std::shared_ptr<Connector> ConnectorPtr;

class TcpClient : noncopyable
{
 public:
  // TcpClient(EventLoop* loop);
  // TcpClient(EventLoop* loop, const string& host, uint16_t port);
  TcpClient(EventLoop* loop,
            const InetAddress& serverAddr,
            const string& nameArg);
  ~TcpClient();  // force out-line dtor, for std::unique_ptr members.

  void connect();
  void disconnect();
  void stop();

  TcpConnectionPtr connection() const
  {
    MutexLockGuard lock(mutex_);
    return connection_;
  }

  EventLoop* getLoop() const { return loop_; }
  bool retry() const { return retry_; }
  void enableRetry() { retry_ = true; }

  const string& name() const
  { return name_; }

  /// Set connection callback.
  /// Not thread safe.
  void setConnectionCallback(ConnectionCallback cb)
  { connectionCallback_ = std::move(cb); }

  /// Set message callback.
  /// Not thread safe.
  void setMessageCallback(MessageCallback cb)
  { messageCallback_ = std::move(cb); }

  /// Set write complete callback.
  /// Not thread safe.
  void setWriteCompleteCallback(WriteCompleteCallback cb)
  { writeCompleteCallback_ = std::move(cb); }

  // LOCAL DELTA:连接失败回调(补上游 TcpClient.cc 的 FIXME setConnectFailedCallback).
  // 每次连接尝试失败都在 loop 线程回调一次(参数是 errno 或 SO_ERROR),语义见 Connector.h.
  // 只转存到 connector_,本类不加数据成员:TcpClient 被使用方按值嵌入,万一游戏代码按上游头、
  // libmuduo_net 按本 overlay 编译,布局不同就是静默 ABI 错配;只加成员函数则只会编译报错.
  // 在 connect() 之前设置(与上面几个 setter 一样不是线程安全的).
  typedef std::function<void (int savedErrno)> ConnectFailedCallback;
  void setConnectFailedCallback(ConnectFailedCallback cb);

 private:
  /// Not thread safe, but in loop
  void newConnection(int sockfd);
  /// Not thread safe, but in loop
  void removeConnection(const TcpConnectionPtr& conn);

  EventLoop* loop_;
  ConnectorPtr connector_; // avoid revealing Connector
  const string name_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  bool retry_;   // atomic
  bool connect_; // atomic
  // always in loop thread
  int nextConnId_;
  mutable MutexLock mutex_;
  TcpConnectionPtr connection_ GUARDED_BY(mutex_);
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_TCPCLIENT_H
