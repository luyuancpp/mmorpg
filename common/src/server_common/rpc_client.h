#ifndef SRC_SERVER_RPCCLIENT_RPC_STUB_CLIENT_H_
#define SRC_SERVER_RPCCLIENT_RPC_STUB_CLIENT_H_

#include <memory>

#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpConnection.h"

#include "src/event/event.h"
#include "src/server_common/rpc_channel.h"
#include "rpc_connection_event.h"

using namespace muduo;
using namespace muduo::net;

namespace common
{
class RpcClient : noncopyable
{
public:
    RpcClient(EventLoop* loop,
        const InetAddress& serverAddr)
        : client_(loop, serverAddr, "RpcClient"),
          channel_(new RpcChannel),
          emp_(EventManager::New())
    {
        client_.setConnectionCallback(
            std::bind(&RpcClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&RpcChannel::onMessage, get_pointer(channel_), _1, _2, _3));
        client_.enableRetry();
    }

    template <typename E, typename Receiver>
    void subscribe(Receiver& receiver) { emp_->subscribe<E>(receiver); }

    void registerService(google::protobuf::Service* service)
    {
        const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
        services_[desc->full_name()] = service;
    }

    void connect()
    {
        channel_->setServices(&services_);
        client_.connect();
    }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            conn->setTcpNoDelay(true);
            channel_->setConnection(conn);
        }
        emp_->emit<RegisterStubES>(conn, channel_);
        emp_->emit<ClientConnectionES>(conn);
    }

    TcpClient client_;
    RpcChannelPtr channel_;
    EventManagerPtr emp_;
    std::map<std::string, ::google::protobuf::Service*> services_;
};

using RpcClientPtr = std::unique_ptr<common::RpcClient>;

} // namespace common

#endif // SRC_SERVER_RPCCLIENT_RPC_STUB_CLIENT_H_
