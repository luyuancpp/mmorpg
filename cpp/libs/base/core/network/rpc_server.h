// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef COMMON_SRC_GAME_RPC_rpc_server_H_
#define COMMON_SRC_GAME_RPC_rpc_server_H_

#include "muduo/net/TcpServer.h"

namespace google {
namespace protobuf {

class Service;

}  // namespace protobuf
}  // namespace google

namespace muduo
{
namespace net
{

class RpcServer
{
 public:
  RpcServer(EventLoop* loop,
            const InetAddress& listenAddr);

  void registerService(::google::protobuf::Service*);
  void start();
    
    [[nodiscard]] TcpServer& GetTcpServer() 
    {
        return tcpServer;
    }
    
 private:
  void onConnection(const TcpConnectionPtr& conn);

  // void onMessage(const TcpConnectionPtr& connection,
  //                Buffer* buf,
  //                Timestamp time);

  TcpServer tcpServer;



  std::map<std::string, ::google::protobuf::Service*> services_;
};

}  // namespace net
}  // namespace muduo

#endif  // COMMON_SRC_GAME_RPC_rpc_server_H_
