#ifndef SRC_SERVER_RPCCLIENT_RPC_CONNECTION_EVENT_H_
#define SRC_SERVER_RPCCLIENT_RPC_CONNECTION_EVENT_H_

#include "muduo/net/TcpConnection.h"

#include "src/server_common/rpc_channel.h"

namespace common
{
    struct RegisterStubEvent
    {
        RegisterStubEvent(const muduo::net::TcpConnectionPtr& conn, muduo::net::RpcChannelPtr& channel)
            : conn_(conn),
              channel_(channel)
        {
        }

        const muduo::net::TcpConnectionPtr& conn_;
        muduo::net::RpcChannelPtr& channel_;

    };

    struct RpcClientConnectionEvent
    {
        RpcClientConnectionEvent(const muduo::net::TcpConnectionPtr& conn)
            : conn_(conn)
        {   
        }
        const muduo::net::TcpConnectionPtr& conn_;
    };

    struct ServerConnectionEvent
    {
        ServerConnectionEvent(const muduo::net::TcpConnectionPtr& conn)
            : conn_(conn)
        {
        }
        const muduo::net::TcpConnectionPtr& conn_;
    };

}//namespace common

#endif//SRC_SERVER_RPCCLIENT_RPC_CONNECTION_EVENT_H_
