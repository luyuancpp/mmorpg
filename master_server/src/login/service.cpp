#include "service.h"

#include "src/common_type/common_type.h"
#include "src/server_registry/server_registry.h"
#include "src/master_player/master_player_list.h"

namespace l2ms
{
    void l2ms::LoginServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
        const ::l2ms::EnterGameRequest* request,
        ::l2ms::EnterGameResponse* response,
        ::google::protobuf::Closure* done)
    {
        auto e = common::reg().create();
        common::reg().emplace<common::GameGuid>(e, request->player_id());
        common::reg().emplace<std::string>(e, request->account());
        master::MasterPlayerList::GetSingleton().EnterGame(request->player_id(), e);
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



