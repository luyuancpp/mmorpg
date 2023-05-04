#include "game_service_replied.h"

#include <boost/get_pointer.hpp>
#include <muduo/base/Logging.h>

#include "src/comp/player_list.h"
#include "src/pb/pbc/service.h"
#include "src/handler/player_service_replied.h"
#include "src/thread_local/controller_thread_local_storage.h"

using MessageUnqiuePtr = std::unique_ptr<google::protobuf::Message>;

void OnGsCallPlayerReplied(const TcpConnectionPtr& conn, const NodeServiceMessageResponsePtr& replied, Timestamp timestamp)
{
    auto it = controller_tls.player_list().find(replied->ex().player_id());
    if (it == controller_tls.player_list().end())
    {
        LOG_ERROR << "PlayerService player not found " << replied->ex().player_id() << ","
            << replied->descriptor()->full_name() << " service " << replied->msg().service();
        return;
    }
    auto service_it = g_player_service_replieds.find(replied->msg().service());
    if (service_it == g_player_service_replieds.end())
    {
        LOG_ERROR << "PlayerService service not found " << replied->ex().player_id() << "," << replied->msg().service();
        return;
    }
    auto& serviceimpl = service_it->second;
    google::protobuf::Service* service = serviceimpl->service();
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(replied->msg().method());
    if (nullptr == method)
    {
        LOG_ERROR << "PlayerService method not found " << replied->msg().method();
        //todo client error;
        return;
    }
    MessageUnqiuePtr player_response(service->GetResponsePrototype(method).New());
    player_response->ParseFromString(replied->msg().body());
    serviceimpl->CallMethod(method, it->second,  boost::get_pointer(player_response));
}

