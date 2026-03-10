#include "gate_node.h"

#include <thread>
#include <chrono>

#include "grpc_client/login/login_grpc_client.h"
#include "core/network/rpc_session.h"
#include "proto/login/login.grpc.pb.h"
#include "proto/common/base/node.pb.h"
#include "rpc/service_metadata/service_metadata.h"
#include "node/system/node/node_util.h"
#include "session/system/session.h"
#include "grpc_client/grpc_init_client.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "session/manager/session_manager.h"

GateNode* gGateNode = nullptr;

GateNode::GateNode(EventLoop* loop)
    : Node(loop, "logs/gate"),
    dispatcher_(std::bind(&GateNode::OnUnknownMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)),
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
		auto it = tlsSessionManager.sessions().find(sessionDetails->session_id());
		if (it == tlsSessionManager.sessions().end()){
			return;
		}
		gGateNode->SendMessageToClient(it->second.conn, reply);
		};
	SetIfEmptyHandler(sendGrpcResponseToClientSession);
}

GateNode::~GateNode() {
    isRunning_ = false;
    if (kafkaConsumerThread_ && kafkaConsumerThread_->joinable()) {
        kafkaConsumerThread_->join();
    }
}

void GateNode::StartRpcServer()
{
	Node::StartRpcServer();

	rpcServer->GetTcpServer().setConnectionCallback(
		std::bind(&GateNode::OnConnection, this, std::placeholders::_1));
	rpcServer->GetTcpServer().setMessageCallback(
		std::bind(&ProtobufCodec::onMessage, &codec_, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

	tlsSessionManager.session_id_gen().set_node_id(GetNodeId());

    // Initialize Kafka Consumer
    ConnectToSceneManager();
}

void GateNode::ConnectToSceneManager() {
    // TODO: Initialize Kafka consumer here
    LOG_INFO << "Initializing Kafka Consumer for Gate " << GetNodeId();
    // kafkaConsumerThread_ = std::make_unique<std::thread>(&GateNode::HandleKafkaMessages, this);
}
