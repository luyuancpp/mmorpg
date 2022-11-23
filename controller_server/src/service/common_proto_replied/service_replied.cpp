#include "service_replied.h"

#include <boost/get_pointer.hpp>
#include <muduo/base/Logging.h>

#include "src/comp/player_list.h"
#include "src/pb/pbc/msgmap.h"
#include "src/service/logic_proto_replied/player_service_replied.h"

using MessageUnqiuePtr = std::unique_ptr<google::protobuf::Message>;

void CallPlayerGsReplied(GsCallPlayerRpc rpc)
{
    auto it = g_players.find(rpc->s_rp_->ex().player_id());
    if (it == g_players.end())
    {
        LOG_ERROR << "PlayerService player not found " << rpc->s_rp_->ex().player_id() << ","
            << rpc->s_rp_->descriptor()->full_name() << " msgid " << rpc->s_rp_->msg().msg_id();
        return;
    }
    auto msg_id = rpc->s_rp_->msg().msg_id();
    auto sit = g_serviceinfo.find(msg_id);
    if (sit == g_serviceinfo.end())
    {
        LOG_ERROR << "PlayerService msg not found " << rpc->s_rp_->ex().player_id() << "," << msg_id;
        return;
    }
    auto service_it = g_player_service_replieds.find(sit->second.service);
    if (service_it == g_player_service_replieds.end())
    {
        LOG_ERROR << "PlayerService service not found " << rpc->s_rp_->ex().player_id() << "," << msg_id;
        return;
    }
    auto& serviceimpl = service_it->second;
    google::protobuf::Service* service = serviceimpl->service();
    const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
    const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(sit->second.method);
    if (nullptr == method)
    {
        LOG_ERROR << "PlayerService method not found " << msg_id;
        //todo client error;
        return;
    }
    MessageUnqiuePtr player_response(service->GetResponsePrototype(method).New());
    player_response->ParseFromString(rpc->s_rp_->msg().body());
    serviceimpl->CallMethod(method, it->second,  boost::get_pointer(player_response));
}

