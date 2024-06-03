#pragma once

#include "src/network/codec/codec.h"

#include "muduo/net/EventLoop.h"

#include "src/network/codec/dispatcher.h"
#include "src/network/rpc_msg_route.h"
#include "src/network/rpc_client.h"

#include "common_proto/database_service.pb.h"
#include "common_proto/c2gate.pb.h"

using RpcClientMessagePtr = std::shared_ptr<ClientRequest>;

class ClientReceiver : muduo::noncopyable
{
public:

    ClientReceiver(ProtobufCodec& codec, ProtobufDispatcher& dispatcher);

    static RpcClientPtr& GetLoginNode(uint64_t session_id);
    ProtobufCodec& codec() const { return codec_; }

    void OnConnection(const muduo::net::TcpConnectionPtr& conn);

    void Send2Client(muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& message) { codec_.send(conn, message); }

    //client to gate 
	void OnRpcClientMessage(const muduo::net::TcpConnectionPtr& conn,
		const RpcClientMessagePtr& message,
		muduo::Timestamp);

    inline uint64_t tcp_session_id(const muduo::net::TcpConnectionPtr& conn) { return boost::any_cast<uint64_t>(conn->getContext()); }

    static void Tip(const muduo::net::TcpConnectionPtr& conn, uint32_t tip_id);
private:
    ProtobufCodec& codec_;
    ProtobufDispatcher& dispatcher_;   
};
