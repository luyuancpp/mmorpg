#ifndef GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_
#define GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_

#include "src/network/codec/codec.h"

#include "muduo/base/noncopyable.h"
#include "muduo/net/EventLoop.h"

#include "src/network/codec/dispatcher.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_stub.h"
#include "src/network/rpc_client.h"
#include "src/network/gate_player_list.h"
#include "src/util/snow_flake.h"

#include "c2gw.pb.h"
#include "login_service.pb.h"
#include "ms_service.pb.h"
#include "gs_service.pb.h"

using namespace muduo;
using namespace muduo::net;

extern std::unordered_set<Guid> g_connected_ids;

template <typename ClientResponse, typename ServerRequest, typename ServerResponse>
struct ClosureReplied
{
	ClosureReplied(const muduo::net::TcpConnectionPtr& cc)
		: s_rp_(new ServerResponse()),
		client_conn_(cc),
        session_id_(boost::any_cast<uint64_t>(client_conn_->getContext())) {}
    ~ClosureReplied() { if (client_conn_.use_count() == 1) { g_connected_ids.erase(session_id_); } }
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
    using RpcStubgw2l = RpcStub<gw2l::LoginService_Stub>;

    ClientReceiver(ProtobufCodec& codec, ProtobufDispatcher& dispatcher);

    RpcStubgw2l& login_stub();

    void OnConnection(const muduo::net::TcpConnectionPtr& conn);

    void Send2Client(muduo::net::TcpConnectionPtr& conn, const ::google::protobuf::Message& messag) { codec_.send(conn, messag); }

    //client to gateway 
    void OnLogin(const muduo::net::TcpConnectionPtr& conn,
        const LoginRequestPtr& message,
        muduo::Timestamp);

    using LoginRpc = std::shared_ptr<ClosureReplied<LoginResponse, gw2l::LoginRequest, gw2l::LoginResponse>>;
    void OnServerLoginReplied(LoginRpc replied);

    void OnCreatePlayer(const muduo::net::TcpConnectionPtr& conn, 
        const CreatePlayerRequestPtr& message, 
        muduo::Timestamp);

    using CreatePlayeRpc = std::shared_ptr<ClosureReplied<CreatePlayerResponse, gw2l::CreatePlayerRequest, gw2l::CreatePlayerResponse>>;
    void OnServerCreatePlayerReplied(CreatePlayeRpc replied);

    void OnEnterGame(const muduo::net::TcpConnectionPtr& conn,
        const EnterGameRequestPtr& message,
        muduo::Timestamp);

    using EnterGameRpc = std::shared_ptr<ClosureReplied<EnterGameResponse, gw2l::EnterGameRequest, gw2l::EnterGameResponse>>;
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


#endif // GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_