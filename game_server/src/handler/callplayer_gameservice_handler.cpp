#include "game_service.pb.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"

///<<< BEGIN WRITING YOUR CODE
#include "src/thread_local/game_thread_local_storage.h"
#include "src/handler/player_service.h"
#include "src/pb/pbc/service.h"

///<<< END WRITING YOUR CODE
void GameService::CallPlayer(::google::protobuf::RpcController* controller,
	const ::NodeServiceMessageRequest* request,
	::NodeServiceMessageResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	auto it = game_tls.player_list().find(request->ex().player_id());
	if (it == game_tls.player_list().end())
	{
		LOG_ERROR << "PlayerService player not found " << request->ex().player_id() << ","
			<< request->descriptor()->full_name() << " msgid " << request->msg().service_method_id();
		return;
	}
	auto sit = g_service_method_info.find(request->msg().service_method_id());
	if (sit == g_service_method_info.end())
	{
		LOG_ERROR << "PlayerService msg not found " << request->ex().player_id() << "," << request->msg().service_method_id();
		return;
	}
	auto service_it = g_player_services.find(sit->second.service);
	if (service_it == g_player_services.end())
	{
		LOG_ERROR << "PlayerService service not found " << request->ex().player_id() << "," << request->msg().service_method_id();
		return;
	}
	auto& serviceimpl = service_it->second;
	google::protobuf::Service* service = serviceimpl->service();
	const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
	const google::protobuf::MethodDescriptor* method = desc->FindMethodByName(sit->second.method);
	if (nullptr == method)
	{
		LOG_ERROR << "PlayerService method not found " << request->msg().service_method_id();
		//todo client error;
		return;
	}
	MessageUnqiuePtr player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(request->msg().body());
	MessageUnqiuePtr player_response(service->GetResponsePrototype(method).New());
	serviceimpl->CallMethod(method, it->second, get_pointer(player_request), get_pointer(player_response));
	if (nullptr == response)
	{
		return;
	}
	response->mutable_msg()->set_body(player_response->SerializeAsString());
	response->mutable_ex()->set_player_id(request->ex().player_id());
	response->mutable_msg()->set_service_method_id(request->msg().service_method_id());
///<<< END WRITING YOUR CODE
}
