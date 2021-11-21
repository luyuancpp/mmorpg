#ifndef SRC_GATEWAY_SERVICE_SERVICE_H_
#define SRC_GATEWAY_SERVICE_SERVICE_H_

#include "src/server_common/rpc_string_closure.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_stub.h"
#include "src/server_common/rpc_client.h"
#include "src/redis_client/redis_client.h"
#include "src/account_player/account_player.h"

#include "gw2l.pb.h"
#include "l2db.pb.h"
#include "l2ms.pb.h"

namespace gw2l
{
    using common::RedisClientPtr;
    class LoginServiceImpl : public LoginService
    {
    public:
        using MessagePtr = std::unique_ptr<google::protobuf::Message>;
        using PlayerPtr = std::shared_ptr<AccountPlayer>;
        using LoginPlayersMap = std::unordered_map<std::string, PlayerPtr>;
        using ConnectionAccountMap = std::unordered_map<common::GameGuid, PlayerPtr>;
        using LoginStubl2ms = common::RpcStub<l2ms::LoginService_Stub>;
        using LoginStubl2db = common::RpcStub<l2db::LoginService_Stub>;

        LoginServiceImpl(LoginStubl2ms& l2ms_login_stub,
            LoginStubl2db& l2db_login_stub);
  
        void set_redis_client(RedisClientPtr& p)
        {
            redis_ = p;
        }

        virtual void Login(::google::protobuf::RpcController* controller,
            const gw2l::LoginRequest* request,
            gw2l::LoginResponse* response,
            ::google::protobuf::Closure* done)override;

        using LoginRpcString = common::RpcString<l2db::LoginRequest,
            l2db::LoginResponse,
            gw2l::LoginResponse>;
        using LoginRP = std::shared_ptr<LoginRpcString>;
        void DbLoginReplied(LoginRP d);

        using LoginMasterRpcString = common::RpcString<l2ms::LoginRequest,
            l2ms::LoginResponse,
            gw2l::LoginResponse>;
        using LoginMasterRP = std::shared_ptr<LoginMasterRpcString>;
        void MasterLoginReplied(LoginMasterRP d);

        virtual void CreatPlayer(::google::protobuf::RpcController* controller,
            const gw2l::CreatePlayerRequest* request,
            gw2l::CreatePlayerResponse* response,
            ::google::protobuf::Closure* done)override;

        using CreatePlayerRpcString = common::RpcString<l2db::CreatePlayerRequest,
            l2db::CreatePlayerResponse,
            gw2l::CreatePlayerResponse>;
        using CreatePlayerRP = std::shared_ptr<CreatePlayerRpcString>;
        void DbCreatePlayerReplied(CreatePlayerRP d);

        virtual void EnterGame(::google::protobuf::RpcController* controller,
            const ::gw2l::EnterGameRequest* request,
            ::gw2l::EnterGameResponse* response,
            ::google::protobuf::Closure* done)override;

        using EnterGameDbRpcString = common::RpcString<l2db::EnterGameRequest,
            l2db::EnterGameResponse,
            gw2l::EnterGameResponse>;
        using EnterGameDbRP = std::shared_ptr<EnterGameDbRpcString>;
        void EnterGameDbReplied(EnterGameDbRP d);

        using EnterGameMasterRpcString = common::RpcString<l2ms::EnterGameRequest,
            l2ms::EnterGameResponese,
            gw2l::EnterGameResponse>;
        using EnterGameMasterRP = std::shared_ptr<EnterGameMasterRpcString>;
        void EnterGameMasterReplied(EnterGameMasterRP d);

        void EnterMasterServer(common::GameGuid guid,
            const std::string& account,
            ::gw2l::EnterGameResponse* response,
            ::google::protobuf::Closure* done);

        void LeaveGame(::google::protobuf::RpcController* controller,
            const ::gw2l::LeaveGameRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;

        virtual void Disconnect(::google::protobuf::RpcController* controller,
            const ::gw2l::DisconnectRequest* request,
            ::google::protobuf::Empty* response,
            ::google::protobuf::Closure* done)override;

    private:
        void UpdateAccount(const std::string& a, const ::account_database& a_d);
        inline void ErasePlayer(ConnectionAccountMap::iterator& cit) 
        {
            login_players_.erase(cit->second->account());
            connection_accounts_.erase(cit);
        }

        RedisClientPtr redis_;
        ConnectionAccountMap connection_accounts_;
        LoginPlayersMap login_players_;

        LoginStubl2ms& l2ms_login_stub_;
        LoginStubl2db& l2db_login_stub_;
    };

}  // namespace gw2l

#endif // SRC_GATEWAY_SERVICE_SERVICE_H_