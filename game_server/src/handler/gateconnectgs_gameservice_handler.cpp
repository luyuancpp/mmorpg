#include "game_service.pb.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void GameService::GateConnectGs(::google::protobuf::RpcController* controller,
	const ::GameNodeConnectRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	InetAddress session_addr(request->rpc_client().ip(), request->rpc_client().port());
	for (auto e : tls.registry.view<RpcServerConnection>())
	{
		auto& conn = tls.registry.get<RpcServerConnection>(e).conn_;
		if (conn->peerAddress().toIpPort() != session_addr.toIpPort())
		{
			continue;
		}
		auto& gate_node = *tls.registry.emplace<GateNodePtr>(e, std::make_shared<GateNodePtr::element_type>(conn));
		gate_node.node_info_.set_node_id(request->gate_node_id());
		gate_node.node_info_.set_node_type(kGateNode);
		game_tls.gate_node().emplace(request->gate_node_id(), e);
		LOG_INFO << "GateConnectGs gate node id " << request->gate_node_id();
		break;
	}
///<<< END WRITING YOUR CODE
}
