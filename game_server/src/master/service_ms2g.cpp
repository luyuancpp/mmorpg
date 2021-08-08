#include "service_ms2g.h"

#include "src/game_server.h"

namespace ms2g
{
    void Ms2gServiceImpl::Login(::google::protobuf::RpcController* controller, 
        const ::ms2g::LoginRequest* request, 
        ::ms2g::LoginResponse* response, 
        ::google::protobuf::Closure* done)
    {

    }

    void Ms2gServiceImpl::ConnectMasterFinish(::google::protobuf::RpcController* controller, 
        const ::google::protobuf::Empty* request, 
        ::google::protobuf::Empty* response, 
        ::google::protobuf::Closure* done)
    {
        //g_game_server->Register2Master();
    }

}