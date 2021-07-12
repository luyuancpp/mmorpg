#ifndef SRC_SERVER_RPCCLIENT_RPC_CONNECTION_EVENT_H_
#define SRC_SERVER_RPCCLIENT_RPC_CONNECTION_EVENT_H_

#include "muduo/net/TcpConnection.h"

namespace common
{
    struct ConnectionEvent
    {
        ConnectionEvent(const muduo::net::TcpConnectionPtr& conn)
            : conn_(conn)
        {   
        }
        const muduo::net::TcpConnectionPtr& conn_;
    };

}//namespace common

#endif//SRC_SERVER_RPCCLIENT_RPC_CONNECTION_EVENT_H_
