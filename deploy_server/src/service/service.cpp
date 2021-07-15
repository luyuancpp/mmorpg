#include "service.h"

#include "src/rpc_closure_param/closure_auto_done.h"
#include "src/server_type_id/server_type_id.h"

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

}//namespace deploy