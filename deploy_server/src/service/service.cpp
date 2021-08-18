#include "service.h"

#include "src/game_logic/entity_cast.h"
#include "src/server_common/closure_auto_done.h"
#include "src/server_common/deploy_variable.h"

namespace deploy
{
    void DeployServiceImpl::ServerInfo(::google::protobuf::RpcController* controller, 
        const ::deploy::ServerInfoRequest* request, 
        ::deploy::ServerInfoResponse* response, 
        ::google::protobuf::Closure* done)
    {
        common::ClosurePtr cp(done);
        uint32_t server_begin_id = (request->group() - 1) * common::SERVER_ID_GROUP_SIZE + 1;//begin form one
        uint32_t server_end_id = server_begin_id + common::SERVER_ID_GROUP_SIZE;
        std::string where_case = std::to_string(server_begin_id) +  
            std::string(" <= id  ") +
            " and id < " +
            std::to_string(server_end_id);
        database_->LoadAll<::serverinfo_database>(*response, where_case);
    }

    void DeployServiceImpl::StartGameServer(::google::protobuf::RpcController* controller, 
        const ::deploy::StartGameServerRequest* request,
        ::deploy::StartGameServerResponse* response,
        ::google::protobuf::Closure* done)
    {
        common::ClosurePtr cp(done);
        ::serverinfo_database& server_info = *response->mutable_my_info();
        server_info.set_ip(request->my_info().ip());
        auto server_entity = servers_.create(entt::to_entity(reuse_id_.Create()));//server id error
        uint32_t server_id = static_cast<uint32_t>(server_entity);
        server_info.set_id(deploy_server::kLogicBeginId + server_id);
        server_info.set_port(deploy_server::kLogicBeginPort + server_id);
        servers_.emplace<::serverinfo_database>(server_entity, server_info);
    }

}//namespace deploy