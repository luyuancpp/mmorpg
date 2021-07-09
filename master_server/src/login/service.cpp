#include "service.h"

namespace l2ms
{
    void l2ms::LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
        const ::l2ms::EnterGameRequest* request,
        ::l2ms::EnterGameResponse* response,
        ::google::protobuf::Closure* done)
    {
        done->Run();
    }

    void LoginServiceImpl::LeaveGame(::google::protobuf::RpcController* controller, 
        const ::l2ms::LeaveGameRequest* request, 
        ::l2ms::LeaveGameResponse* response,
        ::google::protobuf::Closure* done)
    {
        done->Run();
    }

    void LoginServiceImpl::Disconect(::google::protobuf::RpcController* controller, 
        const ::l2ms::DisconectRequest* request,
        ::l2ms::DisconectResponse* response,
        ::google::protobuf::Closure* done)
    {
        done->Run();
    }

}//namespace master



