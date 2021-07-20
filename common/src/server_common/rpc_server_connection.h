#ifndef COMMON_SRC_SERVER_COMMON_RPC_SERVER_CONNECTION_H_
#define COMMON_SRC_SERVER_COMMON_RPC_SERVER_CONNECTION_H_

#include "muduo/net/TcpConnection.h"

namespace common
{
    struct RpcServerConnection
    {
        RpcServerConnection(const muduo::net::TcpConnectionPtr& conn)
            : conn_(conn)
        {

        }
        muduo::net::TcpConnectionPtr conn_;
    };
}//namespace common

#endif//COMMON_SRC_SERVER_COMMON_RPC_SERVER_CONNECTION_H_
