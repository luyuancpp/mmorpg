#pragma once

#include "network/codec/codec.h"

#include "muduo/net/EventLoop.h"

#include "network/rpc_client.h"
#include "network/codec/dispatcher.h"
#include "type_define/type_define.h"

#include "proto/common/c2gate.pb.h"

using RpcClientMessagePtr = std::shared_ptr<ClientRequest>;

class ClientMessageProcessor : muduo::noncopyable
{
public:

    ClientMessageProcessor(ProtobufCodec& codec, ProtobufDispatcher& dispatcher);

    static entt::entity GetLoginNode(uint64_t session_id);
    ProtobufCodec& codec() const { return codec_; }

    void OnConnection(const muduo::net::TcpConnectionPtr& conn);

    void Send2Client( const muduo::net::TcpConnectionPtr& conn,
        const ::google::protobuf::Message& message) const { codec_.send(conn, message); }

    //client to gate 
	void OnRpcClientMessage(const muduo::net::TcpConnectionPtr& conn,
		const RpcClientMessagePtr& message,
		muduo::Timestamp);

    static inline Guid SessionId(const muduo::net::TcpConnectionPtr& conn) { return boost::any_cast<Guid>(conn->getContext()); }

    static void Tip(const muduo::net::TcpConnectionPtr& conn, uint32_t tip_id);
private:
    static void HandleConnectionEstablished(const muduo::net::TcpConnectionPtr& conn);
    static void HandleDisconnection(const muduo::net::TcpConnectionPtr& conn);
    ProtobufCodec& codec_;
    ProtobufDispatcher& dispatcher_;   
};
