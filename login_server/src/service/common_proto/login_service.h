#pragma once
#include "login_service.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/comp/account_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_closure.h"
#include "src/network/rpc_stub.h"
#include "src/network/rpc_client.h"
#include "src/redis_client/redis_client.h"

#include "login_service.pb.h"
#include "database_service.pb.h"
#include "controller_service.pb.h"

///<<< END WRITING YOUR CODE
class LoginServiceImpl : public loginservice::LoginService{
public:
		///<<< BEGIN WRITING YOUR CODE
		using PlayerPtr = std::shared_ptr<AccountPlayer>;
		using LoginPlayersMap = std::unordered_map<std::string, PlayerPtr>;
		using ConnectionEntityMap = std::unordered_map<Guid, EntityPtr>;
		using LoginStubl2controller = RpcStub<controllerservice::ControllerNodeService_Stub>;
		using LoginStubl2db = RpcStub<dbservice::DbService_Stub>;

		LoginServiceImpl(LoginStubl2controller& l2controller_login_stub,
			LoginStubl2db& l2db_login_stub);

		void set_redis_client(PbSyncRedisClientPtr& p) { redis_ = p; }

		using LoginAccountDbRpc = std::shared_ptr< RpcString<dbservice::LoginRequest, dbservice::LoginResponse, loginservice::LoginResponse>>;
		void LoginAccountDbReplied(LoginAccountDbRpc replied);

		using LoginAcountControllerRpc = std::shared_ptr<RpcString<controllerservice::LoginAccountRequest, controllerservice::LoginAccountResponse, loginservice::LoginResponse>>;
		void LoginAccountControllerReplied(LoginAcountControllerRpc replied);

		using CreatePlayerRpc = std::shared_ptr<RpcString<dbservice::CreatePlayerRequest, dbservice::CreatePlayerResponse, loginservice::CreatePlayerResponse>>;
		void CreatePlayerDbReplied(CreatePlayerRpc replied);

		using EnterGameDbRpc = std::shared_ptr<RpcString<dbservice::EnterGameRequest, dbservice::EnterGameResponse, loginservice::EnterGameResponse>>;
		void EnterGameDbReplied(EnterGameDbRpc replied);

		using EnterGameControllerRpc = std::shared_ptr<RpcString<controllerservice::EnterGameRequest, controllerservice::EnterGameResponese, loginservice::EnterGameResponse>>;
		void EnterGameReplied(EnterGameControllerRpc replied);

		void EnterGame(Guid player_id,
			uint64_t connection_id,
			::loginservice::EnterGameResponse* response,
			::google::protobuf::Closure* done);

	private:
		void UpdateAccount(uint64_t connection_id, const ::account_database& a_d);

		PbSyncRedisClientPtr redis_;
		ConnectionEntityMap sessions_;
		LoginStubl2controller& controller_node_stub_;
		LoginStubl2db& l2db_login_stub_;
		///<<< END WRITING YOUR CODE
public:
    void Login(::google::protobuf::RpcController* controller,
        const loginservice::LoginRequest* request,
        loginservice::LoginResponse* response,
        ::google::protobuf::Closure* done)override;

    void CreatPlayer(::google::protobuf::RpcController* controller,
        const loginservice::CreatePlayerRequest* request,
        loginservice::CreatePlayerResponse* response,
        ::google::protobuf::Closure* done)override;

    void EnterGame(::google::protobuf::RpcController* controller,
        const loginservice::EnterGameRequest* request,
        loginservice::EnterGameResponse* response,
        ::google::protobuf::Closure* done)override;

    void LeaveGame(::google::protobuf::RpcController* controller,
        const loginservice::LeaveGameRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

    void Disconnect(::google::protobuf::RpcController* controller,
        const loginservice::DisconnectRequest* request,
        ::google::protobuf::Empty* response,
        ::google::protobuf::Closure* done)override;

};