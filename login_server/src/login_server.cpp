#include "login_server.h"

#include "src/game_config/deploy_json.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/gate_node.h"
#include "src/network/node_info.h"
#include "src/network/rpc_connection_event.h"
#include "src/network/server_component.h"
#include "src/pb/pbc/deploy_service_service.h"
#include "src/pb/pbc/service.h"
#include "src/thread_local/login_thread_local_storage.h"

#include "common.pb.h"

LoginServer* g_login_node = nullptr;


LoginServer::LoginServer(muduo::net::EventLoop* loop)
    : loop_(loop)
{
}

void LoginServer::Init()
{
    g_login_node = this;
    GameConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
    node_info_.set_node_type(kLoginNode);
    node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());
    void InitRepliedHandler();
    InitRepliedHandler();
    InitMessageInfo();
    ConnectDeploy();
}

void LoginServer::ConnectDeploy()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_session_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_session_->subscribe<OnConnected2ServerEvent>(*this);
    deploy_session_->connect();
}

void LoginServer::Start()
{
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

void LoginServer::receive(const OnConnected2ServerEvent& es)
{
    if (!es.conn_->connected())
    {
        return;
    }
    // started 
    if (nullptr != server_)
    {
        return;
    }
    ServerInfoRequest rq;
    rq.set_group(GameConfig::GetSingleton().config_info().group_id());
    deploy_session_->CallMethod(DeployServiceServerInfoMsgId, rq);
}


void LoginServer::receive(const OnBeConnectedEvent& es)
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

		auto gate_node = tls.registry.try_get<GateNodePtr>(e);
		if (nullptr != gate_node && (*gate_node)->node_info_.node_type() == kGateNode)
		{
			//todo
            login_tls.gate_nodes().erase((*gate_node)->node_info_.node_id());
		}

		tls.registry.destroy(e);
		break;
	}

}