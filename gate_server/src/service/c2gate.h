#pragma once

#include "src/network/codec/codec.h"

#include "muduo/base/noncopyable.h"
#include "muduo/net/EventLoop.h"

#include "src/network/codec/dispatcher.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_stub.h"
#include "src/network/rpc_client.h"
#include "src/network/gate_player_list.h"
#include "src/util/snow_flake.h"

#include "c2gate.pb.h"
#include "login_service.pb.h"
#include "controller_service.pb.h"
#include "gs_service.pb.h"

using namespace muduo;
using namespace muduo::net;

template <typename ClientResponse, typename ServerRequest, typename ServerResponse>
struct ClosureReplied
{
	ClosureReplied(const muduo::net::TcpConnectionPtr& cc)
		: s_rp_(new ServerResponse()),
		client_conn_(cc),
        session_id_(boost::any_cast<uint64_t>(client_conn_->getContext())) {}
    ~ClosureReplied() {}
    inline Guid session_id()const { return session_id_; }
	ClientResponse c_rp_;
	ServerRequest s_rq_;
	ServerResponse* s_rp_{ nullptr };
	const muduo::net::TcpConnectionPtr client_conn_;
    Guid session_id_{ kInvalidGuid };
};

using LoginRequestPtr = std::shared_ptr<LoginRequest> ;
using CreatePlayerRequestPtr = std::shared_ptr<CreatePlayerRequest>;
using EnterGameRequestPtr = std::shared_ptr<EnterGameRequest>;
using LeaveGameRequestPtr = std::shared_ptr<LeaveGameRequest>;
using RpcClientMessagePtr = std::shared_ptr<ClientRequest>;

class ClientReceiver : muduo::noncopyable
{
public:
    using RpcStubgw2l = RpcStub<loginservice::LoginService_Stub>;

    ClientReceiver(ProtobufCodec& codec, ProtobufDispatcher& dispatcher);

    RpcStubgw2l& login_stub();
    RpcStubgw2l& login_stub(uint64_t session_id);
    uint32_t find_valid_login_node_id(uint64_t session_id);

    void OnConnection(const muduo::net::TcpConnectionPtr& conn);

    void Send2Client(muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& messag) { codec_.send(conn, messag); }

    //client to gate 
    void OnLogin(const muduo::net::TcpConnectionPtr& conn,
        const LoginRequestPtr& message,
        muduo::Timestamp);

    using LoginRpc = std::shared_ptr<ClosureReplied<LoginResponse, loginservice::LoginRequest, loginservice::LoginResponse>>;
    void OnServerLoginReplied(LoginRpc replied);

    void OnCreatePlayer(const muduo::net::TcpConnectionPtr& conn, 
        const CreatePlayerRequestPtr& message, 
        muduo::Timestamp);

    using CreatePlayeRpc = std::shared_ptr<ClosureReplied<CreatePlayerResponse, loginservice::CreatePlayerRequest, loginservice::CreatePlayerResponse>>;
    void OnServerCreatePlayerReplied(CreatePlayeRpc replied);

    void OnEnterGame(const muduo::net::TcpConnectionPtr& conn,
        const EnterGameRequestPtr& message,
        muduo::Timestamp);

    using EnterGameRpc = std::shared_ptr<ClosureReplied<EnterGameResponse, loginservice::EnterGameRequest, loginservice::EnterGameResponse>>;
    void OnServerEnterGameReplied(EnterGameRpc replied);

    void OnLeaveGame(const muduo::net::TcpConnectionPtr& conn,
        const LeaveGameRequestPtr& message,
        muduo::Timestamp);

	void OnRpcClientMessage(const muduo::net::TcpConnectionPtr& conn,
		const RpcClientMessagePtr& message,
		muduo::Timestamp);

    
    struct ClientGsRpcClosure
    {
        ClientGsRpcClosure(const muduo::net::TcpConnectionPtr& cc)
			: client_connection_(cc){}
        google::protobuf::Message* c_rp_{nullptr};
        const muduo::net::TcpConnectionPtr client_connection_;
    };

	using GsPlayerServiceRpc = std::shared_ptr<ClosureReplied<MessageBody, gsservice::RpcClientRequest, gsservice::RpcClientResponse>>;
	void OnGsPlayerServiceReplied(GsPlayerServiceRpc replied);

    inline uint64_t tcp_session_id(const muduo::net::TcpConnectionPtr& conn) { return boost::any_cast<uint64_t>(conn->getContext()); }
private:
    ProtobufCodec& codec_;
    ProtobufDispatcher& dispatcher_;   
};
