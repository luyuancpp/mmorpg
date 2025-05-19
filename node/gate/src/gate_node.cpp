#include "gate_node.h"

#include <grpcpp/grpcpp.h>

#include "grpc/generator/proto/common/login_service_grpc.h"
#include "log/constants/log_constants.h"
#include "log/system/console_log_system.h"
#include "muduo/base/TimeZone.h"
#include "network/network_constants.h"
#include "network/rpc_session.h"
#include "proto/common/deploy_service.grpc.pb.h"
#include "proto/common/login_service.grpc.pb.h"
#include "proto/logic/constants/node.pb.h"
#include "service_info/centre_service_service_info.h"
#include "service_info/game_service_service_info.h"
#include "service_info/service_info.h"
#include "thread_local/storage_gate.h"
#include "time/system/time_system.h"
#include "util/network_utils.h"
#include "node/comp/node_comp.h"
#include "node/system/node_system.h"

GateNode* gGateNode = nullptr; 

muduo::AsyncLogging& logger()
{
    return  gGateNode->Log();
}

GateNode::GateNode(EventLoop* loop)
    : Node(loop, "logs/gate"),
    dispatcher_(std::bind(&GateNode::OnUnknownMessage, this, _1, _2, _3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    rpcClientHandler(codec_, dispatcher_)
{ }

GateNode::~GateNode()
{
}

void GateNode::Initialize()
{
	GetNodeInfo().set_node_type(GateNodeService);
    allowedTargetNodeTypes = { CentreNodeService, SceneNodeService, LoginNodeService };

    gGateNode = this;

    Node::Initialize();

    void InitGrpcLoginSercieResponseHandler();
    InitGrpcLoginSercieResponseHandler();

}

void GateNode::StartRpcServer()
{

	rpcServer->GetTcpServer().setConnectionCallback(
		std::bind(&GateNode::OnConnection, this, _1));
	rpcServer->GetTcpServer().setMessageCallback(
		std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));

	tls_gate.session_id_gen().set_node_id(GetNodeId());

	//Connect2Login();

	Node::StartRpcServer(); 

	loginGrpcSelectTimer.RunEvery(0.01, []() {
		HandleLoginServiceCompletedQueueMessage(tls.loginNodeRegistry);
		});

    LOG_INFO << "gate node  start at" << GetNodeInfo().DebugString();
}

void GateNode::ProcessGrpcNode(const NodeInfo& nodeInfo)
{
	auto& registry = NodeSystem::GetRegistryForNodeType(nodeInfo.node_type());
	switch (eNodeType::LoginNodeService)
	{
	case eNodeType::LoginNodeService:
	{
		auto loginNodeId = registry.create(entt::entity{ nodeInfo.node_id() });
		auto& channel = registry.get<std::shared_ptr<grpc::Channel>>(loginNodeId);
		registry.emplace<GrpcLoginServiceStubPtr>(loginNodeId,
			LoginService::NewStub(channel));
		//todo 如果重连后连上了不同的gate会不会有异步问题
		tls_gate.login_consistent_node().add(nodeInfo.node_id(),
			loginNodeId);
		break;
	}
	default:
		break;
	}
}

void GateNode::ProcessNodeStop(const NodeInfo& nodeInfo)
{
	tls_gate.login_consistent_node().remove(nodeInfo.node_id());
}
