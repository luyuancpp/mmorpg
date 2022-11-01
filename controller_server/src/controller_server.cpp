#include "controller_server.h"

#include "muduo/base/Logging.h"
#include "muduo//net/EventLoop.h"

#include "src/game_config/all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/region_config.h"

#include "src/network/gate_node.h"
#include "src/network/gs_node.h"
#include "src/game_logic/game_registry.h"
#include "src/service/logic/player_service.h"
#include "src/service/logic/server_service.h"
#include "src/pb/pbc/msgmap.h"

#include "gs_service.pb.h"
#include "gw_service.pb.h"
#include "component_proto/player_network_comp.pb.h"

using namespace muduo;
using namespace net;

ControllerServer* g_controller_node = nullptr;


void set_server_squence_node_id(uint32_t node_id);

uint32_t controller_node_id()
{
	return g_controller_node->controller_node_id();
}

ControllerServer::ControllerServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<PbSyncRedisClientPtr::element_type>())
{ 
    global_entity() = registry.create();
}    

void ControllerServer::Init()
{
    g_controller_node = this;
    InitConfig();
    InitMsgService();
    InitPlayerServcie();

    //connect 
    Connect2Deploy();
}

void ControllerServer::Connect2Deploy()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_session_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_session_->subscribe<RegisterStubEvent>(deploy_stub_);
    deploy_session_->subscribe<OnConnected2ServerEvent>(*this);
    deploy_session_->connect();
}

void ControllerServer::StartServer(ServerInfoRpc replied)
{
    serverinfos_ = replied->s_rp_->info();
    auto& databaseinfo = serverinfos_.database_info();
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    db_session_ = std::make_unique<RpcClient>(loop_, database_addr);
    db_session_->subscribe<RegisterStubEvent>(db_node_stub_);
    db_session_->connect();    

    Connect2Region();
	
    auto& myinfo = serverinfos_.controller_info();
    InetAddress controller_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, controller_addr);
    server_->subscribe<OnBeConnectedEvent>(*this);
    server_->registerService(&contoller_service_);
    for (auto& it : g_server_nomal_service)
    {
        server_->registerService(it.get());
    }
    server_->start();
    LOG_INFO << "controller start " << myinfo.DebugString();
}

void ControllerServer::SceneSqueueNodeId(SceneNodeSequeIdRpc replied)
{
	set_server_squence_node_id(replied->s_rp_->node_id());
}

void ControllerServer::LetGateConnect2Gs(entt::entity gs, entt::entity gate)
{
    auto& connection_info = registry.get<InetAddress>(gs);
    gwservice::StartGSRequest request;
    request.set_ip(connection_info.toIp());
    request.set_port(connection_info.port());
    request.set_gs_node_id(registry.get<GsNodePtr>(gs)->node_id());
	registry.get<GateNodePtr>(gate)->session_.Send(request);
}

void ControllerServer::receive(const OnConnected2ServerEvent& es)
{
	auto& conn = es.conn_;
    if (conn->connected())
    {
		// started 
		if (nullptr == server_)
		{
			{
                ServerInfoRpc rpc(std::make_shared<ServerInfoRpc::element_type>());
                rpc->s_rq_.set_group(GameConfig::GetSingleton().config_info().group_id());
                rpc->s_rq_.set_region_id(RegionConfig::GetSingleton().config_info().region_id());
                deploy_stub_.CallMethod(
                    &ControllerServer::StartServer,
                    rpc,
                    this,
                    &deploy::DeployService_Stub::ServerInfo);
			}
			
            {
                SceneNodeSequeIdRpc rpc(std::make_shared<SceneNodeSequeIdRpc::element_type>());
                deploy_stub_.CallMethod(
                    &ControllerServer::SceneSqueueNodeId,
                    rpc,
                    this,
                    &deploy::DeployService_Stub::SceneSqueueNodeId);
            }
		}
		
		if (nullptr != region_session_)
		{
			if (conn->connected() && IsSameAddr(region_session_->peer_addr(), conn->peerAddress()))
			{
				EventLoop::getEventLoopOfCurrentThread()->queueInLoop(std::bind(&ControllerServer::Register2Region, this));
			}
			else if(!conn->connected() && IsSameAddr(region_session_->peer_addr(), conn->peerAddress()))
			{

			}
		}
    }
	else
	{
		
	}
	
}

void ControllerServer::receive(const OnBeConnectedEvent& es)
{
    auto& conn = es.conn_;
    if (conn->connected())
    {
		auto e = registry.create();
		registry.emplace<RpcServerConnection>(e, RpcServerConnection{ conn });
    }
    else
    {
		auto& peer_addr = conn->peerAddress();
		for (auto e : registry.view<RpcServerConnection>())
		{
			auto& local_addr = registry.get<RpcServerConnection>(e).conn_->peerAddress();
			if (local_addr.toIpPort() != peer_addr.toIpPort())
			{
				continue;
			}
            auto gsnode = registry.try_get<GsNodePtr>(e);//如果是游戏逻辑服则删除
            if (nullptr != gsnode && (*gsnode)->node_info_.node_type() == kGsNode)
            {
                //remove AfterChangeGsEnterScene
				//todo 
                g_gs_nodes.erase((*gsnode)->node_info_.node_id());
            }
			auto gatenode = registry.try_get<GateNodePtr>(e);//如果是gate
			if (nullptr != gatenode && (*gatenode)->node_info_.node_type() == kGatewayNode)
			{
				//todo
                g_gate_nodes.erase((*gatenode)->node_info_.node_id());
			}
			registry.destroy(e);
			break;
		}
    }
}

void ControllerServer::InitConfig()
{
    GameConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
    RegionConfig::GetSingleton().Load("region.json");
    LoadAllConfigAsyncWhenServerLaunch();
}

void ControllerServer::Connect2Region()
{
	auto& regioninfo = serverinfos_.regin_info();
	InetAddress region_addr(regioninfo.ip(), regioninfo.port());
	region_session_ = std::make_unique<RpcClient>(loop_, region_addr);
	region_session_->subscribe<RegisterStubEvent>(rg_stub_);
	region_session_->registerService(&contoller_service_);
	region_session_->subscribe<OnConnected2ServerEvent>(*this);
	region_session_->connect();
}

void ControllerServer::Register2Region()
{
    auto& myinfo = serverinfos_.controller_info();
    regionservcie::StartMsRequest rq;
	auto session_info = rq.mutable_rpc_client();
	auto node_info = rq.mutable_rpc_server();
	session_info->set_ip(region_session_->local_addr().toIp());
	session_info->set_port(region_session_->local_addr().port());
	node_info->set_ip(myinfo.ip());
	node_info->set_port(myinfo.port());
	rq.set_ms_node_id(myinfo.id());
	rg_stub_.CallMethod(
		rq,
		&regionservcie::RgService_Stub::StartMs);
}