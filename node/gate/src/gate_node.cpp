﻿#include "gate_node.h"

#include <grpcpp/grpcpp.h>

#include "grpc/generator/proto/common/login_service_grpc.h"
#include "network/rpc_session.h"
#include "proto/common/login_service.grpc.pb.h"
#include "proto/logic/constants/node.pb.h"
#include "service_info/service_info.h"
#include "thread_local/storage_gate.h"
#include "node/system/node_system.h"

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

	void InitGrpcLoginServiceResponseHandler();
	InitGrpcLoginServiceResponseHandler();
}

void GateNode::StartRpcServer()
{
	Node::StartRpcServer();

	rpcServer->GetTcpServer().setConnectionCallback(
		std::bind(&GateNode::OnConnection, this, _1));
	rpcServer->GetTcpServer().setMessageCallback(
		std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

	tls_gate.session_id_gen().set_node_id(GetNodeId());

	loginGrpcSelectTimer.RunEvery(0.01, []() {
		loginpb::HandleLoginServiceCompletedQueueMessage(tls.GetNodeRegistry(eNodeType::LoginNodeService));
		});

    LOG_INFO << "gate node  start at" << GetNodeInfo().DebugString();
}

void GateNode::ProcessGrpcNode(const NodeInfo& nodeInfo)
{
	auto& registry = tls.GetNodeRegistry(nodeInfo.node_type());
	switch (nodeInfo.node_type())
	{
	case eNodeType::LoginNodeService:
	{
		const auto loginNodeId = entt::entity{ nodeInfo.node_id() };
		const auto& channel = registry.get<std::shared_ptr<grpc::Channel>>(loginNodeId);
		registry.emplace < loginpb::LoginServiceStubPtr > (loginNodeId,
			loginpb::LoginService::NewStub(channel));
		//todo 如果重连后连上了不同的gate会不会有异步问题
		tls_gate.login_consistent_node().add(nodeInfo.node_id(),
			loginNodeId);
		loginpb::InitLoginServiceCompletedQueue(registry, loginNodeId);
		break;
	}
	default:
		break;
	}
}

 void GateNode::ProcessNodeStop(uint32_t nodeType, uint32_t nodeId) 
 {
	 switch (nodeType)
	 {
	 case eNodeType::LoginNodeService:
		 tls_gate.login_consistent_node().remove(nodeId);
	 default:
		 break;
	 }
}
