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

Node::CanConnectNodeTypeList GateNode::GetAllowedTargetNodeTypes()
{
    return { CentreNodeService, SceneNodeService }; 
}

void GateNode::Initialize()
{
	GetNodeInfo().set_node_type(GateNodeService);

    gGateNode = this;

    Node::Initialize();

    void InitGrpcLoginSercieResponseHandler();
    InitGrpcLoginSercieResponseHandler();

}

void GateNode::StartRpcServer()
{
	Node::StartRpcServer(); 

    rpcServer->GetTcpServer().setConnectionCallback(
        std::bind(&GateNode::OnConnection, this, _1));
    rpcServer->GetTcpServer().setMessageCallback(
        std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    
    tls_gate.session_id_gen().set_node_id(GetNodeId());
    
    Connect2Login();

    LOG_INFO << "gate node  start at" << GetNodeInfo().DebugString();
}

void GateNode::Connect2Login()
{
	auto& serviceNodeList = tls.globalNodeRegistry.get<ServiceNodeList>(GlobalGrpcNodeEntity());

    for (auto& loginNodeInfo : serviceNodeList[LoginNodeService].node_list())
    {
        entt::entity id{ loginNodeInfo.node_id() };
        auto loginNodeId = tls_gate.loginNodeRegistry.create(id);
        if (loginNodeId != id)
        {
			LOG_ERROR << "Login node not found for entity: " << entt::to_integral(loginNodeId);
            continue;
        }

        auto channel = grpc::CreateChannel(::FormatIpAndPort(loginNodeInfo.endpoint().ip(), loginNodeInfo.endpoint().port()), 
            grpc::InsecureChannelCredentials());
        tls_gate.loginNodeRegistry.emplace<GrpcLoginServiceStubPtr>(loginNodeId,
            LoginService::NewStub(channel));
        tls_gate.login_consistent_node().add(loginNodeInfo.node_id(),
            loginNodeId);

        InitLoginServiceCompletedQueue(tls_gate.loginNodeRegistry, loginNodeId);
    }

    loginGrpcSelectTimer.RunEvery(0.01, []() {
        HandleLoginServiceCompletedQueueMessage(tls_gate.loginNodeRegistry);
        });

}
