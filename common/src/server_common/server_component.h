#ifndef COMMON_SRC_SERVER_COMMON_RPC_SERVER_CONNECTION_H_
#define COMMON_SRC_SERVER_COMMON_RPC_SERVER_CONNECTION_H_

#include "muduo/net/TcpConnection.h"

#include "src/server_common/rpc_channel.h"

namespace common
{
    struct RpcServerConnection
    {
        RpcServerConnection(const muduo::net::TcpConnectionPtr& conn)
            : conn_(conn),
              channel_(boost::any_cast<muduo::net::RpcChannelPtr>(conn->getContext())){}

        bool Connected() const { return conn_->connected(); }

        void Send(const ::google::protobuf::Message& request,
            const std::string service_name,
            std::string method_name)
        {
            channel_->S2C(request, service_name, method_name);
        }

        muduo::net::TcpConnectionPtr conn_;
    private:
        muduo::net::RpcChannelPtr channel_;
    };
}//namespace common

#endif//COMMON_SRC_SERVER_COMMON_RPC_SERVER_CONNECTION_H_
