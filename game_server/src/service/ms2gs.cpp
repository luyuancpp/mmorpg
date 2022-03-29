#include "ms2gs.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"
#include "src/server_common/rpc_closure.h"

///<<< BEGIN WRITING YOUR CODE 
#include "muduo/base/Logging.h"

#include "src/game_logic/comp/player_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_server.h"
#include "src/module/player_list/player_list.h"
#include "src/pb/pbc/msgmap.h"
#include "src/service/player_service.h"
using namespace game;
///<<< END WRITING YOUR CODE

using namespace common;
namespace ms2gs{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE

/// ///<<<rpc begin
void Ms2gServiceImpl::EnterGame(::google::protobuf::RpcController* controller,
    const ms2gs::EnterGameRequest* request,
    ms2gs::EnterGameRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE EnterGame
    auto it =  g_players.emplace(request->player_id(), common::EntityPtr());
    if (it.second)
    {
        auto entity = it.first->second.entity();
        reg.emplace<GateConnId>(entity, request->conn_id());
        reg.emplace<common::Guid>(entity, request->player_id());
    }
///<<< END WRITING YOUR CODE EnterGame
}

void Ms2gServiceImpl::PlayerService(::google::protobuf::RpcController* controller,
    const ms2gs::Ms2GsPlayerMessageRequest* request,
    ms2gs::Ms2GsPlayerMessageRespone* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerService
    auto& message_extern = request->request_extern();
    auto& player_msg = request->msg();
	auto it = g_players.find(message_extern.player_id());
	if (it == g_players.end())
	{
        LOG_INFO << "player not found " << message_extern.player_id();
        return;
	}
    auto msg_id = request->msg().msg_id();
    if (msg_id >= g_serviceinfo.size() || nullptr == g_serviceinfo[msg_id].method)
    {
        LOG_INFO << "msg not found " << msg_id;
        return;
    }
	auto service_it = g_player_services.find(g_serviceinfo[msg_id].service);
	if (service_it == g_player_services.end())
	{
        LOG_INFO << "msg not found " << msg_id;
		return;
	}
    auto& serviceimpl = service_it->second;
	google::protobuf::Service* service = serviceimpl->service();
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(g_serviceinfo[msg_id].method);
	if (nullptr == method)
	{
        LOG_INFO << "msg not found " << msg_id;
		//todo client error;
		return;
	}
	std::unique_ptr<google::protobuf::Message> player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(player_msg.body());
	std::unique_ptr<google::protobuf::Message> player_response(service->GetResponsePrototype(method).New());
    serviceimpl->CallMethod(method, it->second, get_pointer(player_request), get_pointer(player_response));
    if (nullptr == response)//不需要回复
    {
        return;
    }
	response->mutable_request_extern()->set_player_id(request->request_extern().player_id());
	response->mutable_msg()->set_body(player_response->SerializeAsString());
    response->mutable_msg()->set_msg_id(msg_id);
///<<< END WRITING YOUR CODE PlayerService
}

void Ms2gServiceImpl::PlayerServiceNoRespone(::google::protobuf::RpcController* controller,
    const ms2gs::Ms2GsPlayerMessageRequest* request,
    ::google::protobuf::Empty* response,
    ::google::protobuf::Closure* done)
{
    AutoRecycleClosure d(done);
///<<< BEGIN WRITING YOUR CODE PlayerServiceNoRespone
///<<< END WRITING YOUR CODE PlayerServiceNoRespone
}

///<<<rpc end
}// namespace ms2gs
