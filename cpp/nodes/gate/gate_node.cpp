#include "gate_node.h"

#include <grpcpp/grpcpp.h>

#include "grpc_client/login/login_grpc_client.h"
#include "core/network/rpc_session.h"
#include "proto/login/login.grpc.pb.h"
#include "proto/common/base/node.pb.h"
#include "rpc/service_metadata/service_metadata.h"
#include "node/system/node/node_util.h"
#include "session/system/session.h"
#include "grpc_client/grpc_init_client.h"
#include "session/manager/session_manager.h"

GateNode* gGateNode = nullptr; 

GateNode::GateNode(EventLoop* loop)
    : Node(loop, "logs/gate"),
    dispatcher_(std::bind(&GateNode::OnUnknownMessage, this, _1, _2, _3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    rpcClientHandler(codec_, dispatcher_)
{
	gGateNode = this;
	GetNodeInfo().set_node_type(GateNodeService);
	targetNodeTypeWhitelist = { CentreNodeService, RoomNodeService, LoginNodeService };

	auto sendGrpcResponseToClientSession = [](const ClientContext& context, const ::google::protobuf::Message& reply) {
		auto sessionDetails = GetSessionDetailsByClientContext(context);
		if (nullptr == sessionDetails) {
			return;
		}
		auto it = tlsSessionManager.sessions().find(sessionDetails->session_id());
		if (it == tlsSessionManager.sessions().end()){
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

	tlsSessionManager.session_id_gen().set_node_id(GetNodeId());

}
