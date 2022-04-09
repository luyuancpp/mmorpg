#ifndef SRC_SERVER_RPCCLIENT_RPC_CONNECTION_EVENT_H_
#define SRC_SERVER_RPCCLIENT_RPC_CONNECTION_EVENT_H_

#include "muduo/net/TcpConnection.h"

#include "src/network/rpc_channel.h"

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

    struct OnConnected2ServerEvent
    {
        OnConnected2ServerEvent(const muduo::net::TcpConnectionPtr& conn)
            : conn_(conn)
        {   
        }
        const muduo::net::TcpConnectionPtr& conn_;
    };

    struct OnBeConnectedEvent//服务器被连接
    {
        OnBeConnectedEvent(const muduo::net::TcpConnectionPtr& conn)
            : conn_(conn)
        {
        }
        const muduo::net::TcpConnectionPtr& conn_;
    };

}//namespace common

#endif//SRC_SERVER_RPCCLIENT_RPC_CONNECTION_EVENT_H_
