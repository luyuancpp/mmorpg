#include "gate_node.h"

#include <grpcpp/grpcpp.h>

#include "grpc/generator/proto/login/login_service_grpc.h"
#include "network/rpc_session.h"
#include "proto/login/login_service.grpc.pb.h"
#include "proto/common/node.pb.h"
#include "service_info/service_info.h"
#include "thread_local/storage_gate.h"
#include "node/system/node_util.h"
#include "game_common_logic/system/session_system.h"
#include "grpc/generator/grpc_init.h"

GateNode* gGateNode = nullptr; 

GateNode::GateNode(EventLoop* loop)
    : Node(loop, "logs/gate"),
    dispatcher_(std::bind(&GateNode::OnUnknownMessage, this, _1, _2, _3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    rpcClientHandler(codec_, dispatcher_)
{
	gGateNode = this;
	GetNodeInfo().set_node_type(GateNodeService);
	targetNodeTypeWhitelist = { CentreNodeService, SceneNodeService, LoginNodeService };

	auto sendGrpcResponseToClientSession = [](const ClientContext& context, const ::google::protobuf::Message& reply) {
		auto sessionDetails = GetSessionDetailsByClientContext(context);
		if (nullptr == sessionDetails) {
			return;
		}
		auto it = tls_gate.sessions().find(sessionDetails->session_id());
		if (it == tls_gate.sessions().end()){
			return;
		}
		gGateNode->SendMessageToClient(it->second.conn, reply);
		};
	SetIfEmptyHandler(sendGrpcResponseToClientSession);
}

void GateNode::StartRpcServer()
{
	Node::StartRpcServer();

	rpcServer->GetTcpServer().setConnectionCallback(
		std::bind(&GateNode::OnConnection, this, _1));
	rpcServer->GetTcpServer().setMessageCallback(
		std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

	tls_gate.session_id_gen().set_node_id(GetNodeId());

}
