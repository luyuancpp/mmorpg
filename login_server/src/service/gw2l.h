#ifndef LOGIN_SERVER_SRC_SERVICE_GW2L_H_
#define LOGIN_SERVER_SRC_SERVICE_GW2L_H_
#include "gw2l.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/account_player/account_player.h"
#include "src/game_logic/entity/entity.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_stub.h"
#include "src/server_common/rpc_client.h"
#include "src/redis_client/redis_client.h"

#include "gw2l.pb.h"
#include "l2db.pb.h"
#include "ms_node.pb.h"
///<<< END WRITING YOUR CODE
namespace gw2l{
 ///<<< BEGIN WRITING YOUR CODE
    using common::RedisClientPtr;
 ///<<< END WRITING YOUR CODE
class LoginServiceImpl : public LoginService{
public:
///<<< BEGIN WRITING YOUR CODE
	using PlayerPtr = std::shared_ptr<AccountPlayer>;
	using LoginPlayersMap = std::unordered_map<std::string, PlayerPtr>;
	using ConnectionEntityMap = std::unordered_map<common::Guid, common::EntityPtr>;
	using LoginStubl2ms = common::RpcStub<msservice::MasterNodeService_Stub>;
	using LoginStubl2db = common::RpcStub<l2db::LoginService_Stub>;

	LoginServiceImpl(LoginStubl2ms& l2ms_login_stub,
		LoginStubl2db& l2db_login_stub);

	void set_redis_client(RedisClientPtr& p) { redis_ = p; }

	using LoginRpcReplied = std::shared_ptr< common::RpcString<l2db::LoginRequest, l2db::LoginResponse, gw2l::LoginResponse>>;
	void LoginAccountDbReplied(LoginRpcReplied d);

	using LoginMasterRP = std::shared_ptr<common::RpcString<msservice::LoginAccountRequest, msservice::LoginAccountResponse, gw2l::LoginResponse>>;
	void LoginAccountMSReplied(LoginMasterRP d);

	using CreatePlayerRpcReplied = std::shared_ptr<common::RpcString<l2db::CreatePlayerRequest, l2db::CreatePlayerResponse, gw2l::CreatePlayerResponse>>;
	void CreatePlayerDbReplied(CreatePlayerRpcReplied d);

	using EnterGameDbRpcReplied = std::shared_ptr<common::RpcString<l2db::EnterGameRequest, l2db::EnterGameResponse, gw2l::EnterGameResponse>>;
	void EnterGameDbReplied(EnterGameDbRpcReplied d);

	using EnterGameMSRpcReplied = std::shared_ptr<common::RpcString<msservice::EnterGameRequest, msservice::EnterGameResponese, gw2l::EnterGameResponse>>;
	void EnterMSReplied(EnterGameMSRpcReplied d);

	void EnterMS(common::Guid guid,
		uint64_t connection_id,
		::gw2l::EnterGameResponse* response,
		::google::protobuf::Closure* done);

private:
	void UpdateAccount(uint64_t connection_id, const ::account_database& a_d);

	RedisClientPtr redis_;
	ConnectionEntityMap connections_;
	LoginStubl2ms& ms_node_stub_;
	LoginStubl2db& l2db_login_stub_;
///<<< END WRITING YOUR CODE
public:
    void Login(::google::protobuf::RpcController* controller,
        const gw2l::LoginRequest* request,
        gw2l::LoginResponse* response,
        ::google::protobuf::Closure* done)override;

    void CreatPlayer(::google::protobuf::RpcController* controller,
        const gw2l::CreatePlayerRequest* request,
        gw2l::CreatePlayerResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterGame(::google::protobuf::RpcController* controller,
        const gw2l::EnterGameRequest* request,
        gw2l::EnterGameResponse* response,
        ::google::protobuf::Closure* done)override;

    void LeaveGame(::google::protobuf::RpcController* controller,
        const gw2l::LeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void Disconnect(::google::protobuf::RpcController* controller,
        const gw2l::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};
}// namespace gw2l
#endif//LOGIN_SERVER_SRC_SERVICE_GW2L_H_
