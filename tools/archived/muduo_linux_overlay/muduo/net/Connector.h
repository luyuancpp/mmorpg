// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

// ---------------------------------------------------------------------------
// LOCAL OVERLAY -- 由 tools/archived/setup_dependencies.sh 在配置 muduo 之前
// 拷到 third_party/muduo-linux/muduo/net/Connector.h。
//
// 除标 LOCAL DELTA 的几处外与上游逐字一致:新增连接失败回调(ConnectFailedCallback /
// setConnectFailedCallback / connectFailedCallback_ / notifyConnectFailed),retry() 多带
// savedErrno。动机与实现见同目录 Connector.cc 的说明。本头是 muduo 内部头,游戏代码不
// include,加数据成员只影响 libmuduo_net 自己。
//
// 升级 muduo 子模块时:用新的上游文件覆盖本文件,再逐处补回 LOCAL DELTA。
// ---------------------------------------------------------------------------

#ifndef MUDUO_NET_CONNECTOR_H
#define MUDUO_NET_CONNECTOR_H

#include "muduo/base/noncopyable.h"
#include "muduo/net/InetAddress.h"

#include <functional>
#include <memory>

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;

class Connector : noncopyable,
                  public std::enable_shared_from_this<Connector>
{
 public:
  typedef std::function<void (int sockfd)> NewConnectionCallback;
  // LOCAL DELTA(连接失败回调,上游没有):每次连接尝试失败时在 loop 线程回调一次,
  // savedErrno 是 connect() 的 errno 或 SO_ERROR(Windows 的 SO_ERROR 是 WSA 错误码),自连接为 0。
  // 回调里可以 stop():回调返回后复核 connect_,已停就不再排下一次重试。
  // 回调不得同步释放 Connector 的最后一个引用(TcpClient 析构时会把它多留 1s)。
  // 回调里也不得同步 start() / restart()(含 TcpClient::connect()):失败时排队的 resetChannel 尚未执行,
  // 当场重连会让它误删新的 channel_(connect() 当场失败时还会递归);要重连请排到下一轮。
  typedef std::function<void (int savedErrno)> ConnectFailedCallback;

  Connector(EventLoop* loop, const InetAddress& serverAddr);
  ~Connector();

  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }
  // LOCAL DELTA:见 ConnectFailedCallback。
  void setConnectFailedCallback(const ConnectFailedCallback& cb)
  { connectFailedCallback_ = cb; }

  void start();  // can be called in any thread
  void restart();  // must be called in loop thread
  void stop();  // can be called in any thread

  const InetAddress& serverAddress() const { return serverAddr_; }

 private:
  enum States { kDisconnected, kConnecting, kConnected };
  static const int kMaxRetryDelayMs = 30*1000;
  static const int kInitRetryDelayMs = 500;

  void setState(States s) { state_ = s; }
  void startInLoop();
  void stopInLoop();
  void connect();
  void connecting(int sockfd);
  void handleWrite();
  void handleError();
  // LOCAL DELTA:多带 savedErrno,转给连接失败回调(stopInLoop 传 0,那时 connect_ 已为 false、不回调)。
  void retry(int sockfd, int savedErrno);
  // LOCAL DELTA
  void notifyConnectFailed(int savedErrno);
  int removeAndResetChannel();
  void resetChannel();

  EventLoop* loop_;
  InetAddress serverAddr_;
  bool connect_; // atomic
  States state_;  // FIXME: use atomic variable
  std::unique_ptr<Channel> channel_;
  NewConnectionCallback newConnectionCallback_;
  // LOCAL DELTA:Connector.h 是内部头(使用方不 include),加数据成员不影响使用方 ABI。
  ConnectFailedCallback connectFailedCallback_;
  int retryDelayMs_;
};

}  // namespace net
}  // namespace muduo

#endif  // MUDUO_NET_CONNECTOR_H
