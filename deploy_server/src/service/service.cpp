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
        ClosurePtr cp(done);
        uint32_t group_id = request->group();
        uint32_t server_begin_id = group_id * kServerSize + kGroupBegin + 1;//begin form one
        uint32_t server_end_id = server_begin_id + kServerSize;
        std::string where_case = std::to_string(server_begin_id) +  
            " <= id  " +
            " and id < " +
            std::to_string(server_end_id);
        database_->LoadAll<::group_server_db>(*response, where_case);

        where_case = std::to_string(group_id + 1) + " = id  ";
        database_->LoadOne(*response->mutable_redis_info(), where_case);

        RegionServer(request->region_id(), response->mutable_regin_info());
    }

    void DeployServiceImpl::StartGameServer(::google::protobuf::RpcController* controller, 
        const ::deploy::StartGameServerRequest* request,
        ::deploy::StartGameServerResponse* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        ::group_server_db& server_info = *response->mutable_my_info();
        auto& rpc_client = request->rpc_client();
        muduo::net::InetAddress ip_port(rpc_client.ip(), rpc_client.port());
        if (server_info.id() > 0)
        {
            g_deploy_server->reuse_game_id().Emplace(ip_port.toIpPort(), server_info.id());
            return;
        }
        server_info.set_ip(request->my_info().ip());
        uint32_t node_id = g_deploy_server->CreateGameServerId();
        LOG_INFO << "new server id " << node_id;
        server_info.set_id(deploy::kLogicBegin + node_id);
        server_info.set_port(deploy::kLogicBeginPort + node_id);

        g_deploy_server->reuse_game_id().Emplace(ip_port.toIpPort(), node_id);
        g_deploy_server->SaveGameServerDb();
        //g_deploy_server->LogReuseInfo();
        response->set_error_no(RET_OK);
    }

    void DeployServiceImpl::StartRegionServer(::google::protobuf::RpcController* controller, 
        const ::deploy::RegionInfoRequest* request,
        ::deploy::RegionInfoResponse* response,
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        RegionServer(request->region_id(), response->mutable_info());
    }

    void DeployServiceImpl::RegionServer(::google::protobuf::RpcController* controller,
        const ::deploy::RegionInfoRequest* request, 
        ::deploy::RegionInfoResponse* response, 
        ::google::protobuf::Closure* done)
    {
        ClosurePtr cp(done);
        RegionServer(request->region_id(), response->mutable_info());
    }

    void DeployServiceImpl::RegionServer(uint32_t region_id,
        ::group_server_db* response)
    {
        uint32_t server_id = (region_id - 1) * kRegionServerSize + kRegionBegin + 1;//
        std::string where_case = std::to_string(server_id) + " = id  ";
        database_->LoadOne(*response, where_case);
    }

}//namespace deploy