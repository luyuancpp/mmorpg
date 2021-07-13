#ifndef SRC_GATEWAY_SERVICE_SERVICE_H_
#define SRC_GATEWAY_SERVICE_SERVICE_H_

#include "src/rpc_closure_param/rpc_string_closure.h"
#include "src/rpc_closure_param/rpc_closure.h"
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

        using EnterGameRpcString = common::RpcString<l2db::EnterGameRequest,
            l2db::EnterGameResponse,
            gw2l::EnterGameResponse>;
        using EnterGameRP = std::shared_ptr<EnterGameRpcString>;
        void EnterGameReplied(EnterGameRP d);

        void EnterMasterServer(common::GameGuid player_id, const std::string& account);

        using EnterMasterGameRpcClosure = common::RpcClosure<l2ms::EnterGameRequest,
            l2ms::EnterGameResponse>;
        using EnterMasterGameRC = std::shared_ptr<EnterMasterGameRpcClosure>;
        void EnterMasterGameReplied(EnterMasterGameRC d);

        virtual void Disconnect(::google::protobuf::RpcController* controller,
            const ::gw2l::DisconnectRequest* request,
            ::gw2l::DisconnectResponse* response,
            ::google::protobuf::Closure* done)override;

        void UpdateAccount(const std::string& a, const ::account_database& a_d);
    private:
        RedisClientPtr redis_;
        ConnectionAccountMap connection_accounts_;
        LoginPlayersMap login_players_;
    };

}  // namespace gw2l

#endif // SRC_GATEWAY_SERVICE_SERVICE_H_