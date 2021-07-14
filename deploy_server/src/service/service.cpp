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
        uint32_t server_id = (request->group() - 1) * common::SERVER_ID_GROUP_SIZE + request->server_type_id();
        response->mutable_info()->set_id(server_id);
        database_->LoadOne(*response->mutable_info());
    }

}//namespace deploy