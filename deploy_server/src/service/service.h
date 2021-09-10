#ifndef DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_
#define DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_

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

        virtual void StartRegionServer(::google::protobuf::RpcController* controller,
            const ::deploy::RegionInfoRequest* request,
            ::deploy::RegionInfoResponse* response,
            ::google::protobuf::Closure* done)override;

        virtual void RegionServer(::google::protobuf::RpcController* controller,
            const ::deploy::RegionInfoRequest* request,
            ::deploy::RegionInfoResponse* response,
            ::google::protobuf::Closure* done)override;

    private:
        void RegionServer(uint32_t region_id, ::group_server_db* response);

        MysqlClientPtr database_;
        GameServerMap logic_server_map_;
        
    };
}//namespace deploy

#endif//DEPLOY_SERVER_SRC_SERVICE_SERVICE_H_
