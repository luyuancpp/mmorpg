#include "login_server.h"

#include <grpcpp/grpcpp.h>

#include "muduo/net/EventLoop.h"

#include "src/game_config/deploy_json.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/gate_node.h"
#include "src/network/node_info.h"
#include "src/network/rpc_connection_event.h"
#include "src/network/server_component.h"
#include "service/service.h"
#include "src/thread_local/login_thread_local_storage.h"
#include "service/grpc/deploy_service.grpc.pb.h"
#include "common_proto/common.pb.h"
#include "common_proto/game_service.pb.h"
#include "src/grpc/deployclient.h"

LoginServer* g_login_node = nullptr;

void AsyncCompleteGrpc();

LoginServer::LoginServer(muduo::net::EventLoop* loop)
    : loop_(loop)
{
}

LoginServer::~LoginServer()
{
	tls.dispatcher.sink<OnConnected2ServerEvent>().disconnect<&LoginServer::Receive1>(*this);
	tls.dispatcher.sink<OnBeConnectedEvent>().disconnect<&LoginServer::Receive2>(*this);
}

void LoginServer::LoadNodeConfig()
{
    ZoneConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
}

void LoginServer::Init()
{
    g_login_node = this;

    InitNodeServer();

    LoadNodeConfig();

    node_info_.set_node_type(kLoginNode);
    node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());

    void InitRepliedHandler();
    InitRepliedHandler();

    InitMessageInfo();
}

void LoginServer::Start()
{
	tls.dispatcher.sink<OnBeConnectedEvent>().connect<&LoginServer::Receive2>(*this);
    server_->registerService(&handler_);
    server_->start();
}

void LoginServer::StartServer(const ::servers_info_data& info)
{
    auto& database_info = info.database_info();
    database_node_info_.set_node_id(database_info.id());
    database_node_info_.set_node_type(kDatabaseNode);
    //route2db.mutable_node_info()->CopyFrom(database_node_info_);
    InetAddress database_addr(database_info.ip(), database_info.port());
    db_session_ = std::make_unique<RpcClient>(loop_, database_addr);
    db_session_->connect();

    auto& controller_info = info.controller_info();
    controller_node_info_.set_node_id(controller_info.id());
    controller_node_info_.set_node_type(kControllerNode);
    //route2controller.mutable_node_info()->CopyFrom(controller_node_info_);
    InetAddress controller_node_addr(controller_info.ip(), controller_info.port());
    controller_session_ = std::make_unique<RpcClient>(loop_, controller_node_addr);
    controller_session_->connect();
    
    auto& redis_info = info.redis_info();
    login_tls.redis().Connect(redis_info.ip(), redis_info.port(), 1, 1);
 
    conf_info_ = info.login_info();
    node_info_.set_node_id(conf_info_.id());
    InetAddress login_addr(conf_info_.ip(), conf_info_.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, login_addr);
   
    Start();
}

void LoginServer::Receive1(const OnConnected2ServerEvent& es) const
{
 
}

void LoginServer::Receive2(const OnBeConnectedEvent& es)
{
	auto& conn = es.conn_;
	if (conn->connected())
	{
		auto e = tls.registry.create();
		tls.registry.emplace<RpcServerConnection>(e, RpcServerConnection{ conn });
		return;
	}

	auto& peer_addr = conn->peerAddress();
	for (auto e : tls.registry.view<RpcServerConnection>())
	{
		auto& local_addr = tls.registry.get<RpcServerConnection>(e).conn_->peerAddress();
		if (local_addr.toIpPort() != peer_addr.toIpPort())
		{
			continue;
		}

		if (const auto gate_node = tls.registry.try_get<GateNodePtr>(e);
			nullptr != gate_node && (*gate_node)->node_info_.node_type() == kGateNode)
		{
			//todo
            login_tls.gate_nodes().erase((*gate_node)->node_info_.node_id());
		}

		tls.registry.destroy(e);
		break;
	}
}

void LoginServer::InitNodeServer()
{
    auto& zone = ZoneConfig::GetSingleton().config_info();

    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    std::string target_str = deploy_info.ip() + ":" + std::to_string(deploy_info.port());
    auto deploy_channel = grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials());
    extern std::unique_ptr<DeployService::Stub> g_deploy_stub;
    g_deploy_stub = DeployService::NewStub(deploy_channel);
    g_deploy_client = std::make_unique_for_overwrite<DeployClient>();
    EventLoop::getEventLoopOfCurrentThread()->runEvery(0.01, AsyncCompleteGrpc);

    NodeInfoRequest req;
    req.set_zone_id(zone.zone_id());
    void SendGetNodeInfo(NodeInfoRequest & req);
    SendGetNodeInfo(req);
}