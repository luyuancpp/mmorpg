#ifndef DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_
#define DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_

#include "src/mysql_database/mysql_database.h"
#include "entt/src/entt/entity/registry.hpp"

#include "deploy.pb.h"

namespace deploy
{
    class DeployServiceImpl : public DeployService
    {
    public:
        using MysqlClientPtr = std::shared_ptr<common::MysqlDatabase>;
        using LogicServerMap = std::unordered_map<uint32_t, ::serverinfo_database>;

        void set_player_mysql_client(MysqlClientPtr& ptr)
        {
            database_ = ptr;
        }

        virtual void ServerInfo(::google::protobuf::RpcController* controller,
            const ::deploy::ServerInfoRequest* request,
            ::deploy::ServerInfoResponse* response,
            ::google::protobuf::Closure* done)override;

        virtual void StartLogicServer(::google::protobuf::RpcController* controller,
            const ::deploy::StartLogicServerRequest* request,
            ::deploy::StartLogicServerResponse* response,
            ::google::protobuf::Closure* done)override;

    private:
        MysqlClientPtr database_;
        LogicServerMap logic_server_map_;
        entt::registry servers_;
    };
}//namespace deploy

#endif//DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_
