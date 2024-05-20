#include "gate_server.h"

#include <grpcpp/grpcpp.h>

#include "src/game_config/deploy_json.h"
#include "src/network/game_node.h"
#include "src/network/login_node.h"
#include "src/network/server_component.h"
#include "service/service.h"
#include "service/controller_service_service.h"
#include "service/grpc/deploy_service.grpc.pb.h"
#include "service/game_service_service.h"
#include "src/thread_local/gate_thread_local_storage.h"
#include "src/grpc/deploy/deployclient.h"
#include "common_proto/game_service.pb.h"
#include "service/login_service_service.h"

GateServer* g_gate_node = nullptr; 

void AsyncCompleteGrpc();

void GateServer::LoadNodeConfig()
{
	ZoneConfig::GetSingleton().Load("game.json");
	DeployConfig::GetSingleton().Load("deploy.json");
}

GateServer::~GateServer()
{
    tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&GateServer::Receive1>(*this);
}

void GateServer::Init()
{
    g_gate_node = this;

    InitNodeServer();

    LoadNodeConfig();

    node_info_.set_node_type(kGateNode);
    node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());

    InitMessageInfo();

    void InitRepliedHandler();
    InitRepliedHandler();

    tls.dispatcher.sink<OnConnected2ServerEvent>().connect<&GateServer::Receive1>(*this);
}

void GateServer::InitNodeServer()
{
    auto& zone = ZoneConfig::GetSingleton().config_info();

    //const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    std::string target_str = "127.0.0.1:1000";
    auto deploy_channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    extern std::unique_ptr<DeployService::Stub> g_deploy_stub;
    g_deploy_stub = DeployService::NewStub(deploy_channel);
    g_deploy_client = std::make_unique_for_overwrite<DeployClient>();
    EventLoop::getEventLoopOfCurrentThread()->runEvery(0.01, AsyncCompleteGrpc);

    for (uint32_t i = 0; i < 1000; ++i)
    {
        NodeInfoRequest req;
        req.set_zone_id(i);
        void SendGetNodeInfo(NodeInfoRequest & req);
        SendGetNodeInfo(req);
    }
   
}

void GateServer::StartServer()
{
    auto& myinfo = conf_info_.gate_info();
    InetAddress gate_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_unique<TcpServer>(loop_, gate_addr, "gate");
    server_->setConnectionCallback(
        std::bind(&GateServer::OnConnection, this, _1));
    server_->setMessageCallback(
        std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    server_->start();

    EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
        [this]() ->void
        {
            /*GroupLignRequest rq;
            rq.set_group_id(GameConfig::GetSingleton().config_info().group_id());
            deploy_session()->CallMethod(DeployServiceLoginNodeInfoMsgId, rq);*/
        }
    );

    EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
        [this]() ->void
        {
            auto& controller_node_info = conf_info_.controller_info();
            InetAddress controller_addr(controller_node_info.ip(), controller_node_info.port());
            controller_node_ = std::make_unique<RpcClient>(loop_, controller_addr);
            controller_node_->registerService(&gate_service_handler_);
            controller_node_->connect();
        }
    );
}


void GateServer::Receive1(const OnConnected2ServerEvent& es)
{
    auto& conn = es.conn_;
    if (IsSameAddr(conn->peerAddress(), DeployConfig::GetSingleton().deploy_info()))
    {
        // started 
        if (nullptr != server_)//断线重连
        {
            return;
        }
        if (!conn->connected())
        {
            return;
        }
        EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
            [this]() ->void
            {
                /*ServerInfoRequest rq;
                rq.set_group(GameConfig::GetSingleton().config_info().group_id());
                deploy_session()->CallMethod(DeployServiceServerInfoMsgId, rq);*/
            }
        );
    }
    else if (IsSameAddr(conn->peerAddress(), conf_info_.controller_info()))
    {
        if (!conn->connected())
        {
            return;
        }
        EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
            [this]() ->void
            {
                auto& controller_node_addr = controller_node_->local_addr();
                GateConnectRequest rq;
                rq.mutable_rpc_client()->set_ip(controller_node_addr.toIp());
                rq.mutable_rpc_client()->set_port(controller_node_addr.port());
                rq.set_gate_node_id(gate_node_id());
                controller_node_session()->CallMethod(ControllerServiceGateConnectMsgId, rq);
            }
        );
    }
    else
    {
        //todo 断线重连
        for (auto& it : gate_tls.game_nodes())
        {
            if (!IsSameAddr(it.second.gs_session_->peer_addr(), conn->peerAddress()))
            {
                continue;
            }
            if (conn->connected())
            {
                auto& game_node = it.second;
                EventLoop::getEventLoopOfCurrentThread()->queueInLoop(
                    [this, &game_node, &conn]() ->void
                    {
                        GameNodeConnectRequest rq;
                        rq.mutable_rpc_client()->set_ip(conn->localAddress().toIp());
                        rq.mutable_rpc_client()->set_port(conn->localAddress().port());
                        rq.set_gate_node_id(gate_node_id());
                        game_node.gs_session_->CallMethod(GameServiceGateConnectGsMsgId, rq);
                    }
                );
            }
            else
            {
                //g_game_nodes.erase()
            }
        }
        for (const auto& it : gate_tls.login_nodes())
        {
            LOG_INFO << it.second.login_session_->peer_addr().toIpPort() << "," << conn->peerAddress().toIpPort();
            if (!IsSameAddr(it.second.login_session_->peer_addr(), conn->peerAddress()))
            {
                continue;
            }
            if (conn->connected())
            {
                EventLoop::getEventLoopOfCurrentThread()->queueInLoop([&it, this]() -> void
                {
                    const auto& login_node_addr = it.second.login_session_->local_addr();
                    GateConnectRequest connect2login_request;
                    connect2login_request.mutable_rpc_client()->set_ip(login_node_addr.toIp());
                    connect2login_request.mutable_rpc_client()->set_port(login_node_addr.port());
                    connect2login_request.set_gate_node_id(gate_node_id());
                    it.second.login_session_->CallMethod(LoginServiceGateConnectMsgId, connect2login_request);
                });
                break;
            }
            gate_tls.login_nodes().erase(it.first);
            break;
        }
    }
}
