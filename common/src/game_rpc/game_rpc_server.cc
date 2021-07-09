// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "game_rpc_server.h"
#include "game_rpc_channel.h"

#include "muduo/base/Logging.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>

using namespace muduo;
using namespace muduo::net;

namespace common
{
RpcServer::RpcServer(EventLoop* loop,
                     const InetAddress& listenAddr)
  : server_(loop, listenAddr, "GameRpcServer")
{
  server_.setConnectionCallback(
      std::bind(&RpcServer::onConnection, this, _1));
//   server_.setMessageCallback(
//       std::bind(&GameRpcServer::onMessage, this, _1, _2, _3));
}

void RpcServer::registerService(google::protobuf::Service* service)
{
  const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
  services_[desc->full_name()] = service;
}

void RpcServer::start()
{
  server_.start();
}

void RpcServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "GameRpcServer - " << conn->peerAddress().toIpPort() << " -> "
    << conn->localAddress().toIpPort() << " is "
    << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    RpcChannelPtr channel(new RpcChannel(conn));
    channel->setServices(&services_);
    conn->setMessageCallback(
        std::bind(&RpcChannel::onMessage, get_pointer(channel), _1, _2, _3));
    conn->setContext(channel);
  }
  else
  {
    conn->setContext(RpcChannelPtr());
    // FIXME:
  }
}

// void GameRpcServer::onMessage(const TcpConnectionPtr& conn,
//                           Buffer* buf,
//                           Timestamp time)
// {
//   RpcChannelPtr& channel = boost::any_cast<RpcChannelPtr&>(conn->getContext());
//   channel->onMessage(conn, buf, time);
// }

}//namespace common