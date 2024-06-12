#include "gate_node.h"

#include <grpcpp/grpcpp.h>

#include "game_config/deploy_json.h"
#include "network/rpc_session.h"
#include "network/node_info.h"
#include "service/service.h"
#include "service/centre_service_service.h"
#include "common_proto/deploy_service.grpc.pb.h"
#include "service/game_service_service.h"
#include "thread_local/gate_thread_local_storage.h"
#include "grpc/deploy/deployclient.h"

#include "common_proto/game_service.pb.h"

GateNode* g_gate_node = nullptr; 

void AsyncCompleteGrpc();

void GateNode::LoadNodeConfig()
{
	ZoneConfig::GetSingleton().Load("game.json");
	DeployConfig::GetSingleton().Load("deploy.json");
}

GateNode::~GateNode()
{
    tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&GateNode::Receive1>(*this);
}

void GateNode::Init()
{
    g_gate_node = this;

    LoadNodeConfig();
    InitNodeByReqInfo();

    node_info_.set_node_type(kGateNode);
    node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());

    InitMessageInfo();

    void InitRepliedHandler();
    InitRepliedHandler();

    tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&GateNode::Receive1>(*this);
}

void GateNode::InitNodeByReqInfo()
{
    auto& zone = ZoneConfig::GetSingleton().config_info();

    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    std::string target_str = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    auto deploy_channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    extern std::unique_ptr<DeployService::Stub> g_deploy_stub;
    g_deploy_stub = DeployService::NewStub(deploy_channel);
    g_deploy_client = std::make_unique_for_overwrite<DeployClient>();
    EventLoop::getEventLoopOfCurrentThread()->runEvery(0.01, AsyncCompleteGrpc);

    {
        NodeInfoRequest req;
        req.set_zone_id(ZoneConfig::GetSingleton().config_info().zone_id());
        void SendGetNodeInfo(NodeInfoRequest & request);
        SendGetNodeInfo(req);
    }
}

void GateNode::StartServer()
{
    auto& gate_info = node_net_info_.gate_info();
    InetAddress gate_addr(gate_info.ip(), gate_info.port());
    server_ = std::make_unique<TcpServer>(loop_, gate_addr, "gate");
    server_->setConnectionCallback(
        std::bind(&GateNode::OnConnection, this, _1));
    server_->setMessageCallback(
        std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    server_->start();

    Connect2Centre();
}


void GateNode::Receive1(const OnConnected2ServerEvent& es)
{
    auto& conn = es.conn_;
    if (conn->connected())
    {
        if (IsSameAddr(conn->peerAddress(), node_net_info_.centre_info()))
        {
            EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
                [this]() ->void
                {
                    RegisterGateRequest rq;
                    rq.mutable_rpc_client()->set_ip(centre_node_->local_addr().toIp());
                    rq.mutable_rpc_client()->set_port(centre_node_->local_addr().port());
                    rq.set_gate_node_id(gate_node_id());
                    centre_node()->CallMethod(CentreServiceRegisterGateMsgId, rq);
                }
            );
            return;
        }
        //todo 断线重连
        for (auto& e : tls.game_node_registry.view<RpcClientPtr>())
        {
            auto& game_node = tls.game_node_registry.get<RpcClientPtr>(e);
            if (!IsSameAddr(game_node->peer_addr(), conn->peerAddress()))
            {
                continue;
            }
            EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
                [this, game_node, conn]() ->void
                {
                    RegisterGateRequest rq;
                    rq.mutable_rpc_client()->set_ip(game_node->local_addr().toIp());
                    rq.mutable_rpc_client()->set_port(game_node->local_addr().port());
                    rq.set_gate_node_id(gate_node_id());
                    game_node->CallMethod(GameServiceRegisterGateMsgId, rq);
                }
            );
        }

    }
    else
    {
        for (auto e : tls.game_node_registry.view<RpcClientPtr>())
        {
            auto& game_node = tls.game_node_registry.get<RpcClientPtr>(e);
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
    auto& centre_node_info = node_net_info_.centre_info();
    InetAddress centre_addr(centre_node_info.ip(), centre_node_info.port());
    centre_node_ = std::make_unique<RpcClient>(loop_, centre_addr);
    centre_node_->registerService(&gate_service_handler_);
    centre_node_->connect();
}
