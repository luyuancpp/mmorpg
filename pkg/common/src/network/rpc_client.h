#ifndef SRC_SERVER_RPCCLIENT_RPC_STUB_CLIENT_H_
#define SRC_SERVER_RPCCLIENT_RPC_STUB_CLIENT_H_

#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpConnection.h"

#include "network/game_channel.h"
#include "thread_local/storage.h"
#include "rpc_connection_event.h"

using namespace muduo;
using namespace muduo::net;

//todo 
class RpcClient : noncopyable
{
public:
    RpcClient(EventLoop* loop,
        const InetAddress& serverAddr)
        : client_(loop, serverAddr, "RpcClient"),
          channel_(new GameChannel)
    {
        client_.setConnectionCallback(
            std::bind(&RpcClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&GameChannel::HandleIncomingMessage, get_pointer(channel_), _1, _2, _3));
        client_.enableRetry();
    }

	const InetAddress& local_addr()const 
    {
		if (nullptr == client_.connection())
        {
            static InetAddress s;
            return s;
		}

        return client_.connection()->localAddress(); 
    }

    const InetAddress& peer_addr()const 
    { 
        if (nullptr == client_.connection())
        {
			static InetAddress s;
			return s;
		}

        return client_.connection()->peerAddress(); 
    }

    //bool connected()const { return nullptr != client_.connection(); }
    inline bool connected()const { return connected_; }    
    
    void registerService(google::protobuf::Service* service)
    {
        const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
        services_[desc->full_name()] = service;
    }

    void connect()
    {
        channel_->SetServiceMap(&services_);
        client_.connect();
    }

    void CallRemoteMethod(uint32_t message_id, const ::google::protobuf::Message& request)
	{
		if (!connected_)
		{
			LOG_ERROR << "client disconnect";
			return;
		}

		channel_->CallRemoteMethod(message_id, request);
	}

    void SendRequest(uint32_t message_id, const ::google::protobuf::Message& message)
    {
        if (!connected_)
        {
            LOG_ERROR << "client disconnect";
            return;
        }

        channel_->SendRequest(message_id, message);
    }

    void RouteMessageToNode(uint32_t message_id,  const ::google::protobuf::Message& request)
    {
        if (!connected_)
        {
            LOG_ERROR << "client disconnect";
            return;
        }

        channel_->RouteMessageToNode(message_id, request);
    }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            conn->setTcpNoDelay(true);
            channel_->SetConnection(conn);
            connected_ = true;
        }
        else
        {
            connected_ = false;
        }
        tls.dispatcher.trigger<OnConnected2ServerEvent>(conn);
    }

    bool connected_{ false };

    TcpClient client_;
    GameChannelPtr channel_;
    std::map<std::string, ::google::protobuf::Service*> services_;
};

using RpcClientPtr = std::shared_ptr<RpcClient>;

#endif // SRC_SERVER_RPCCLIENT_RPC_STUB_CLIENT_H_
