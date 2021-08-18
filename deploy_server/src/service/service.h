#ifndef DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_
#define DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_

#include "entt/src/entt/entity/registry.hpp"
#include "src/game_logic/reuse_id/reuse_id.h"
#include "src/mysql_database/mysql_database.h"

#include "deploy.pb.h"

namespace deploy
{
    class DeployServiceImpl : public DeployService
    {
    public:
        using ServerId = uint32_t;
        using MysqlClientPtr = std::shared_ptr<common::MysqlDatabase>;
        using GameServerMap = std::unordered_map<uint32_t, ::group_server_db>;

        void set_player_mysql_client(MysqlClientPtr& ptr)
        {
            database_ = ptr;
        }

        virtual void ServerInfo(::google::protobuf::RpcController* controller,
            const ::deploy::ServerInfoRequest* request,
            ::deploy::ServerInfoResponse* response,
            ::google::protobuf::Closure* done)override;

        virtual void StartGameServer(::google::protobuf::RpcController* controller,
            const ::deploy::StartGameServerRequest* request,
            ::deploy::StartGameServerResponse* response,
            ::google::protobuf::Closure* done)override;

    private:
        MysqlClientPtr database_;
        GameServerMap logic_server_map_;
        entt::registry servers_;
        common::ReuseId<ServerId, std::unordered_set<ServerId>, UINT16_MAX> reuse_id_;
    };
}//namespace deploy

#endif//DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_
