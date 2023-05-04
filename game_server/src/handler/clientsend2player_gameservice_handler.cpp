#include "game_service.pb.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"

///<<< BEGIN WRITING YOUR CODE
#include "src/thread_local/game_thread_local_storage.h"
#include "src/handler/player_service.h"
#include "src/pb/pbc/service.h"
///<<< END WRITING YOUR CODE
void GameService::ClientSend2Player(::google::protobuf::RpcController* controller,
	const ::GameNodeRpcClientRequest* request,
	::GameNodeRpcClientResponse* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE

	auto it = g_player_services.find(request->service());
	if (it == g_player_services.end())
	{
		LOG_ERROR << "GatePlayerService service not found " << request->service();
		return;
	}
	google::protobuf::Service* service = it->second->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(request->method());
	if (nullptr == method)
	{
		LOG_ERROR << "GatePlayerService method not found " << request->method();
		return;
	}
	auto cit = game_tls.gate_sessions().find(request->session_id());
	if (cit == game_tls.gate_sessions().end())
	{
		LOG_INFO << "GatePlayerService session not found  " << request->service() << "," << request->method() << "," << request->session_id();
		return;
	}
	auto try_player_id = tls.registry.try_get<Guid>(cit->second);
	if (nullptr == try_player_id)
	{
		LOG_ERROR << "GatePlayerService player not loading";
		return;
	}
	auto pit = game_tls.player_list().find(*try_player_id);
	if (pit == game_tls.player_list().end())
	{
		LOG_ERROR << "GatePlayerService player not found" << *try_player_id;
		return;
	}
	MessageUnqiuePtr player_request(service->GetRequestPrototype(method).New());
	player_request->ParseFromString(request->request());
	MessageUnqiuePtr player_response(service->GetResponsePrototype(method).New());
	it->second->CallMethod(method, pit->second, get_pointer(player_request), get_pointer(player_response));
	response->set_response(player_response->SerializeAsString());
	response->set_service(request->service());
	response->set_method(request->method());
	response->set_id(request->id());
	response->set_session_id(request->session_id());
///<<< END WRITING YOUR CODE
}
