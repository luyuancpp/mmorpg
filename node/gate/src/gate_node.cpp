#include "gate_node.h"

#include <grpcpp/grpcpp.h>

#include "common_proto/deploy_service.grpc.pb.h"
#include "common_proto/login_service.grpc.pb.h"
#include "constants_proto/node.pb.h"
#include "game_config/deploy_json.h"
#include "grpc/deploy/deployclient.h"
#include "network/rpc_session.h"
#include "service/centre_service_service.h"
#include "service/game_service_service.h"
#include "service/service.h"
#include "thread_local/thread_local_storage_gate.h"

GateNode* g_gate_node = nullptr; 

void AsyncCompleteGrpcDeployService();
void AsyncCompleteRpcLoginService();

void AsyncOutput(const char* msg, int len)
{
    g_gate_node->Log().append(msg, len);
}

GateNode::GateNode(EventLoop* loop)
    : loop_(loop),
    log_{"logs/gate", kMaxLogFileRollSize, 1},
    dispatcher_(std::bind(&GateNode::OnUnknownMessage, this, _1, _2, _3)),
    codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
    client_receiver_(codec_, dispatcher_)
{ }

GateNode::~GateNode()
{
    Exit();
}

void GateNode::Init()
{
    g_gate_node = this;
    InitLog();
    InitConfig();
    InitNodeByReqInfo();

    node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());

    InitMessageInfo();

    void InitRepliedHandler();
    InitRepliedHandler();

    tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&GateNode::Receive1>(*this);
}

void GateNode::Exit()
{
    log_.stop();
    tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&GateNode::Receive1>(*this);
}

void GateNode::InitNodeByReqInfo()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    const std::string target_str = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    const auto channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    extern std::unique_ptr<DeployService::Stub> g_deploy_stub;
    g_deploy_stub = DeployService::NewStub(channel);
    g_deploy_cq = std::make_unique_for_overwrite<CompletionQueue>();
    deploy_rpc_timer_.RunEvery(0.001, AsyncCompleteGrpcDeployService);
    {
        NodeInfoRequest rq;
        rq.set_node_type(kGateNode);
        rq.set_zone_id(ZoneConfig::GetSingleton().config_info().zone_id());
        void SendGetNodeInfo(const NodeInfoRequest&);
        SendGetNodeInfo(rq);
    }
}

void GateNode::StartServer(const nodes_info_data& data)
{
    node_net_info_ = std::move(data);
    auto& gate_info = node_net_info_.gate_info().gate_info()[GetNodeConfIndex()];
    InetAddress gate_addr(gate_info.ip(), gate_info.port());
    server_ = std::make_unique<TcpServer>(loop_, gate_addr, "gate");
    server_->setConnectionCallback(
        std::bind(&GateNode::OnConnection, this, _1));
    server_->setMessageCallback(
        std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    server_->start();
    tls_gate.session_id_gen().set_node_id(GetNodeId());
    Connect2Centre();
    Connect2Login();

    LOG_INFO << "gate node  start " << gate_info.DebugString();
    deploy_rpc_timer_.Cancel();
}

void GateNode::SetNodeId(NodeId node_id)
{
    node_info_.set_node_id(node_id);
}

void GateNode::Receive1(const OnConnected2ServerEvent& es) const
{
    if ( auto& conn = es.conn_ ; conn->connected())
    {
        for (auto&& [_, centre_node] : tls.centre_node_registry.view<RpcClientPtr>().each())
        {
            if (IsSameAddr(conn->peerAddress(), centre_node->peer_addr()))
            {
                EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
                    [this, centre_node]() ->void
                    {
                        RegisterGateRequest rq;
                        rq.mutable_rpc_client()->set_ip(centre_node->local_addr().toIp());
                        rq.mutable_rpc_client()->set_port(centre_node->local_addr().port());
                        rq.set_gate_node_id(GetNodeId());
                        centre_node->CallMethod(CentreServiceRegisterGateMsgId, rq);
                    }
                );
                return;
            }
        }

        //todo 断线重连
        for (auto&& [e, game_node] : tls.game_node_registry.view<RpcClientPtr>().each())
        {
            if (!IsSameAddr(game_node->peer_addr(), conn->peerAddress()))
            {
                continue;
            }
            EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
                [this, game_node]() ->void
                {
                    RegisterGateRequest rq;
                    rq.mutable_rpc_client()->set_ip(game_node->local_addr().toIp());
                    rq.mutable_rpc_client()->set_port(game_node->local_addr().port());
                    rq.set_gate_node_id(GetNodeId());
                    game_node->CallMethod(GameServiceRegisterGateMsgId, rq);
                }
            );
        }

    }
    else
    {
        for (const auto& [e, game_node] : tls.game_node_registry.view<RpcClientPtr>().each())
        {
            if (!IsSameAddr(game_node->peer_addr(), conn->peerAddress()))
            {
                continue;
            }
            Destroy(tls.game_node_registry, e);
        }
    }
}

void GateNode::Connect2Centre()
{
    for (auto& centre_node_info : node_net_info_.centre_info().centre_info())
    {
        entt::entity id{ centre_node_info.id() };
        const auto centre_node_id = tls.centre_node_registry.create(id);
        if (centre_node_id != id)
        {
            LOG_ERROR << "centre id ";
            continue;
        }
        InetAddress centre_addr(centre_node_info.ip(), centre_node_info.port());
        auto& centre_node = tls.centre_node_registry.emplace<RpcClientPtr>(centre_node_id,
            std::make_shared<RpcClientPtr::element_type>(loop_, centre_addr));
        centre_node->registerService(&service_handler_);
        centre_node->connect();
        if (centre_node_info.zone_id() == 
            ZoneConfig::GetSingleton().config_info().zone_id())
        {
            zone_centre_node_ = centre_node;
        }
    }
}

void GateNode::Connect2Login() const
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
    }
    EventLoop::getEventLoopOfCurrentThread()->runEvery(0.0001, AsyncCompleteRpcLoginService);
    void InitLoginNodeComponent();
    InitLoginNodeComponent();
}

void GateNode::InitLog()
{
    log_.start();
}

void GateNode::InitConfig ( )
{
    InitNodeConfig();
}

void GateNode::InitNodeConfig()
{
    ZoneConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
}

void
    GateNode::InitGameConfig ( )
{
}
