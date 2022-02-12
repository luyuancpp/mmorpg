#include "service.h"

#include "muduo/base/Logging.h"

#include "src/deploy_server.h"
#include "src/game_logic/entity_cast.h"
#include "src/server_common/closure_auto_done.h"
#include "src/server_common/deploy_variable.h"
#include "src/return_code/error_code.h"

using namespace common;

namespace deploy
{
    void DeployServiceImpl::ServerInfo(::google::protobuf::RpcController* controller, 
        const ::deploy::ServerInfoRequest* request, 
        ::deploy::ServerInfoResponse* response, 
        ::google::protobuf::Closure* done)
    {
        AutoRecycleClosure cp(done);
        auto group_id = request->group();
        auto& servers_info = *response->mutable_info();
        if (group_id > 0)
        {
            std::string where_case = std::to_string(group_id) + " = id  ";
            database_->LoadOne(*servers_info.mutable_database_info(), where_case);
            database_->LoadOne(*servers_info.mutable_login_info(), where_case);
            database_->LoadOne(*servers_info.mutable_master_info(), where_case);
            database_->LoadOne(*servers_info.mutable_gateway_info(), where_case);
            database_->LoadOne(*servers_info.mutable_redis_info(), where_case);
        }
        else
        {
            std::string where_case = std::to_string(request->region_id()) + " = region_id  ";
            database_->LoadAll<::master_server_db>(*response->mutable_region_masters(), where_case);
        }
        RegionServer(request->region_id(), servers_info.mutable_regin_info());
    }

    void DeployServiceImpl::StartGS(::google::protobuf::RpcController* controller, 
        const ::deploy::StartGSRequest* request,
        ::deploy::StartGSResponse* response,
        ::google::protobuf::Closure* done)
    {
        AutoRecycleClosure cp(done);
        auto& server_info = *response->mutable_my_info();
        auto& rpc_client = request->rpc_client();
        muduo::net::InetAddress ip_port(rpc_client.ip(), rpc_client.port());
        if (server_info.id() > 0)
        {
            g_deploy_server->reuse_game_id().Emplace(ip_port.toIpPort(), server_info.id());
            return;
        }
        server_info.set_ip(request->my_info().ip());
        uint32_t node_id = g_deploy_server->CreateGSId();
        LOG_INFO << "new server id " << node_id;
        server_info.set_id(node_id);
        server_info.set_port(node_id + kGSBeginPort);

        g_deploy_server->reuse_game_id().Emplace(ip_port.toIpPort(), node_id);
        g_deploy_server->SaveGSDb();
        //g_deploy_server->LogReuseInfo();
    }

    void DeployServiceImpl::StartRegionServer(::google::protobuf::RpcController* controller, 
        const ::deploy::RegionInfoRequest* request,
        ::deploy::RegionInfoResponse* response,
        ::google::protobuf::Closure* done)
    {
        AutoRecycleClosure cp(done);
        RegionServer(request->region_id(), response->mutable_info());
    }

    void DeployServiceImpl::RegionServer(::google::protobuf::RpcController* controller,
        const ::deploy::RegionInfoRequest* request, 
        ::deploy::RegionInfoResponse* response, 
        ::google::protobuf::Closure* done)
    {
        AutoRecycleClosure cp(done);
        RegionServer(request->region_id(), response->mutable_info());
    }

    void DeployServiceImpl::RegionServer(uint32_t region_id,
        ::region_server_db* response)
    {
        std::string where_case = std::to_string(region_id) + " = id  ";
        database_->LoadOne(*response, where_case);
    }

}//namespace deploy