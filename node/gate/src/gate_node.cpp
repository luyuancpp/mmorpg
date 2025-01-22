#include "gate_node.h"

#include <grpcpp/grpcpp.h>

#include "game_config/deploy_json.h"
#include "grpc/deploy/deploy_client.h"
#include "grpc/generator/deploy_service_grpc.h"
#include "grpc/generator/login_service_grpc.h"
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

GateNode* g_gate_node = nullptr; 

muduo::AsyncLogging& logger()
{
    return  g_gate_node->Log();
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

uint32_t GateNode::GetNodeType() const
{
    return kGateNode;
}

void GateNode::Init()
{
    g_gate_node = this;

    Node::Init();

    void InitGrpcDeploySercieResponseHandler();
    InitGrpcDeploySercieResponseHandler();

    void InitGrpcLoginSercieResponseHandler();
    InitGrpcLoginSercieResponseHandler();

    void InitRepliedHandler();
    InitRepliedHandler();

    tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&GateNode::Receive1>(*this);
}

void GateNode::ShutdownNode()
{
    tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&GateNode::Receive1>(*this);
}

void GateNode::StartRpcServer(const nodes_info_data& data)
{
    node_net_info_ = std::move(data);
    
    auto& gate_info = node_net_info_.gate_info().gate_info()[GetNodeId()];
    InetAddress gate_addr(gate_info.ip(), gate_info.port());
    
    rpcServer = std::make_unique<RpcServerPtr::element_type>(loop_, gate_addr);
    rpcServer->GetTcpServer().setConnectionCallback(
        std::bind(&GateNode::OnConnection, this, _1));
    rpcServer->GetTcpServer().setMessageCallback(
        std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    rpcServer->start();
    
    tls_gate.session_id_gen().set_node_id(GetNodeId());
    
    Connect2Centre();
    Connect2Login();

    LOG_INFO << "gate node  start " << gate_info.DebugString();
    deployRpcTimer.Cancel();
}

void GateNode::Receive1(const OnConnected2ServerEvent& es) 
{
    if ( auto& conn = es.conn_ ; conn->connected())
    {
        for (auto&& [_, centre_node] : tls.centreNodeRegistry.view<RpcClientPtr>().each())
        {
            if (IsSameAddress(conn->peerAddress(), centre_node->peer_addr()))
            {
                EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
                    [this, centre_node]() ->void
                    {
                        RegisterGateNodeRequest rq;
                        rq.mutable_rpc_client()->set_ip(centre_node->local_addr().toIp());
                        rq.mutable_rpc_client()->set_port(centre_node->local_addr().port());
                        rq.set_gate_node_id(GetNodeId());
                        centre_node->CallRemoteMethod(CentreServiceRegisterGateNodeMessageId, rq);
                    }
                );
                return;
            }
        }

        //todo 断线重连
        for (auto&& [e, game_node] : tls.sceneNodeRegistry.view<RpcClientPtr>().each())
        {
            if (!IsSameAddress(game_node->peer_addr(), conn->peerAddress()))
            {
                continue;
            }
            EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
                [this, game_node]() ->void
                {
                    RegisterGateNodeRequest rq;
                    rq.mutable_rpc_client()->set_ip(game_node->local_addr().toIp());
                    rq.mutable_rpc_client()->set_port(game_node->local_addr().port());
                    rq.set_gate_node_id(GetNodeId());
                    game_node->CallRemoteMethod(GameServiceRegisterGateNodeMessageId, rq);
                }
            );
        }

    }
    else
    {
        for (const auto& [e, game_node] : tls.sceneNodeRegistry.view<RpcClientPtr>().each())
        {
            if (!IsSameAddress(game_node->peer_addr(), conn->peerAddress()))
            {
                continue;
            }
            Destroy(tls.sceneNodeRegistry, e);
        }
    }
}

void GateNode::Connect2Centre()
{
    for (auto& centre_node_info : node_net_info_.centre_info().centre_info())
    {
        entt::entity id{ centre_node_info.id() };
        const auto centre_node_id = tls.centreNodeRegistry.create(id);
        if (centre_node_id != id)
        {
            LOG_ERROR << "centre id ";
            continue;
        }
        InetAddress centre_addr(centre_node_info.ip(), centre_node_info.port());
        auto& centre_node = tls.centreNodeRegistry.emplace<RpcClientPtr>(centre_node_id,
            std::make_shared<RpcClientPtr::element_type>(loop_, centre_addr));
        centre_node->registerService(&service_handler_);
        centre_node->connect();
        if (centre_node_info.zone_id() == 
            ZoneConfig::GetSingleton().ConfigInfo().zone_id())
        {
            zone_centre_node_ = centre_node;
        }
    }
}

void GateNode::Connect2Login()
{
    for (auto& login_node_info : node_net_info_.login_info().login_info())
    {
        entt::entity id{ login_node_info.id() };
        auto login_node_id = tls_gate.login_node_registry.create(id);
        if (login_node_id != id)
        {
            LOG_ERROR << "login id ";
            continue;
        }
        auto channel = grpc::CreateChannel(login_node_info.addr(), grpc::InsecureChannelCredentials());
        tls_gate.login_node_registry.emplace<std::unique_ptr<LoginService::Stub>>(login_node_id,
            LoginService::NewStub(channel));
        tls_gate.login_consistent_node().add(login_node_info.id(), 
            login_node_id);
        InitLoginServiceCompletedQueue(tls_gate.login_node_registry, login_node_id);
    }

    loginGrpcSelectTimer.RunEvery(0.01, []() {
        HandleLoginServiceCompletedQueueMessage(tls_gate.login_node_registry);
        });

}
