// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "rpc_server.h"
#include "game_channel.h"

#include "muduo/base/Logging.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/service.h>

#include "rpc_connection_event.h"
#include <thread_local/storage.h>

using namespace muduo;
using namespace muduo::net;

RpcServer::RpcServer(EventLoop* loop,
                     const InetAddress& listenAddr)
    : tcpServer(loop, listenAddr, "RpcServer")
{
  tcpServer.setConnectionCallback(
      std::bind(&RpcServer::onConnection, this, _1));
//   tcpServer.setMessageCallback(
//       std::bind(&RpcServer::onMessage, this, _1, _2, _3));
}

void RpcServer::registerService(google::protobuf::Service* service)
{
  const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
  services_[desc->full_name().data()] = service;
}

void RpcServer::start()
{
  tcpServer.start();
}

void RpcServer::onConnection(const TcpConnectionPtr& conn)
{
    LOG_DEBUG << "RpcServer - " << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    GameChannelPtr channel(new GameChannel(conn));
    channel->SetServiceMap(&services_);
    conn->setMessageCallback(
        std::bind(&GameChannel::HandleIncomingMessage, get_pointer(channel), _1, _2, _3));
    conn->setContext(channel);
  }
  else
  {
    conn->setContext(GameChannelPtr());
    // FIXME:
  }
  tls.dispatcher.trigger<OnTcpClientConnectedEvent>(conn);
}

// void RpcServer::onMessage(const TcpConnectionPtr& conn,
//                           Buffer* buf,
//                           Timestamp time)
// {
//   RpcChannelPtr& channel = boost::any_cast<RpcChannelPtr&>(conn->getContext());
//   channel->onMessage(conn, buf, time);
// }

