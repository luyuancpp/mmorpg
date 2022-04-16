#include "game_server.h"

#include "muduo/base/Logging.h"
#include "muduo/base/CrossPlatformAdapterFunction.h"

#include "src/game_config/all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/region_config.h"

#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/game_registry.h"
#include "src/network/gate_node.h"
#include "src/service/logic/player_service.h"
#include "src/service/replied_ms2gs.h"
#include "src/network/deploy_rpcclient.h"
#include "src/network/node_info.h"
#include "src/pb/pbc/msgmap.h"

using namespace common;

GameServer* g_gs = nullptr;


GameServer::GameServer(muduo::net::EventLoop* loop)
    :loop_(loop),
     redis_(std::make_shared<RedisClient>()){}

void GameServer::Init()
{
    
    GameConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");
    RegionConfig::GetSingleton().Load("region.json");
    InitMsgService();
    loadallconfig();
    InitGlobalEntities();
    InitPlayerServcie();
}

void GameServer::InitNetwork()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_session_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_session_->subscribe<RegisterStubEvent>(deploy_stub_);
    deploy_session_->subscribe<OnConnected2ServerEvent>(*this);
    deploy_session_->connect();
}

void GameServer::ServerInfo(ServerInfoRpcRC cp)
{
    auto& resp = cp->s_rp_;
    auto& info = cp->s_rp_->info();
   
    auto& regioninfo = info.regin_info();
    InetAddress region_addr(regioninfo.ip(), regioninfo.port());
   
    region_session_ = std::make_unique<RpcClient>(loop_, region_addr);
    
    StartGSRpcRC scp(std::make_shared<StartGSInfoRpcClosure>());
    scp->s_rq_.set_group(GameConfig::GetSingleton().config_info().group_id());
    scp->s_rq_.mutable_my_info()->set_ip(muduo::ProcessInfo::localip());
    scp->s_rq_.mutable_my_info()->set_id(server_deploy_.id());
    scp->s_rq_.mutable_rpc_client()->set_ip(deploy_session_->local_addr().toIp());
    scp->s_rq_.mutable_rpc_client()->set_port(deploy_session_->local_addr().port());
    deploy_stub_.CallMethod(
        &GameServer::StartGSDeployReplied,
        scp,
        this,
        &deploy::DeployService_Stub::StartGS);

	RegionRpcClosureRC rcp(std::make_shared<RegionRpcClosureRC::element_type>());
    rcp->s_rq_.set_region_id(RegionConfig::GetSingleton().config_info().region_id());
	deploy_stub_.CallMethod(
		&GameServer::RegionInfoReplied,
        rcp,
		this,
		&deploy::DeployService_Stub::RegionInfo);
}

void GameServer::StartGSDeployReplied(StartGSRpcRC cp)
{
    //uint32_t snid = server_info_.id() - deploy_server::kGameSnowflakeIdReduceParam;//snowflake id 
    ConnectRegion();

    auto& redisinfo = cp->s_rp_->redis_info();
	redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);

    server_deploy_ = cp->s_rp_->my_info();
    InetAddress node_addr(server_deploy_.ip(), server_deploy_.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, node_addr);
    server_->subscribe<OnBeConnectedEvent>(*this);
    server_->registerService(&gs_service_impl_);
    server_->start();   
}

void GameServer::RegionInfoReplied(RegionRpcClosureRC cp)
{
    //connect master
    auto& resp = cp->s_rp_;
	auto& regionmaster = resp->region_masters();
	for (int32_t i = 0; i < regionmaster.masters_size(); ++i)
	{
        auto e = reg.create();
		auto& masterinfo = regionmaster.masters(i);
		InetAddress master_addr(masterinfo.ip(), masterinfo.port());
		auto it = g_ms_nodes.emplace(masterinfo.id(), MsNodePtr(std::make_shared<MsNode>()));
		auto& ms = *it.first->second;
		ms.session_ = std::make_shared<RpcClient>(loop_, master_addr);
		ms.node_info_.set_node_id(masterinfo.id());
		reg.emplace<MsNodeWPtr>(e, it.first->second);

		auto& ms_node = ms.session_;
		ms_node->subscribe<RegisterStubEvent>(g2ms_stub_);
		ms_node->registerService(&gs_service_impl_);
		ms_node->subscribe<OnConnected2ServerEvent>(*this);
		ms_node->connect();
	}
}

void GameServer::Register2Master(MasterSessionPtr& ms_node)
{
    ms2gs::RepliedMs2g::StartGameMasterRpcRC scp(std::make_shared<ms2gs::RepliedMs2g::StartGameMasterRpcClosure>());
    auto& master_local_addr = ms_node->local_addr();
    msservice::StartGSRequest& request = scp->s_rq_;
    auto session_info = request.mutable_rpc_client();
    auto node_info = request.mutable_rpc_server();
    session_info->set_ip(master_local_addr.toIp());
    session_info->set_port(master_local_addr.port());
    node_info->set_ip(server_deploy_.ip());
    node_info->set_port(server_deploy_.port());
    request.set_server_type(reg.get<eServerType>(global_entity()));
    request.set_gs_node_id(server_deploy_.id());
    g2ms_stub_.CallMethod(
        &ms2gs::RepliedMs2g::StartGSMasterReplied,
        scp,
        &ms2gs::RepliedMs2g::GetSingleton(),
        &msservice::MasterNodeService_Stub::StartGS);
}

void GameServer::receive(const OnConnected2ServerEvent& es)
{
    if (deploy_session_->peer_addr().toIpPort() == es.conn_->peerAddress().toIpPort())
    {
        // started 
        if (nullptr != server_)
        {
            return;
        }

        EventLoop::getEventLoopOfCurrentThread()->runInLoop(
            [this]() ->void
            {
                ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
                if (reg.get<eServerType>(global_entity()) == kMainServer)
                {
                    cp->s_rq_.set_group(GameConfig::GetSingleton().config_info().group_id());
                }
                cp->s_rq_.set_region_id(RegionConfig::GetSingleton().config_info().region_id());
                deploy_stub_.CallMethod(
                    &GameServer::ServerInfo,
                    cp,
                    this,
                    &deploy::DeployService_Stub::ServerInfo);
            }
        );
    }

    for (auto e : reg.view<MsNodePtr>())
    {
        auto& ms_node = reg.get<MsNodePtr>(e);
        auto& master_session = ms_node->session_;
        if (es.conn_->connected() &&
            master_session->peer_addr().toIpPort() == es.conn_->peerAddress().toIpPort())
        {
            EventLoop::getEventLoopOfCurrentThread()->runInLoop(std::bind(&GameServer::Register2Master, this, master_session));
            break;
        }
		else if (!es.conn_->connected() &&
			      master_session->peer_addr().toIpPort() == es.conn_->peerAddress().toIpPort())
        {
            g_ms_nodes.erase(ms_node->node_id());
            reg.destroy(e);
            break;
        }
    }

	for (auto& it : g_ms_nodes)
	{
        auto& ms_node = it.second;
		auto& master_session = ms_node->session_;
		if (es.conn_->connected() && 
            master_session->connected() &&
			master_session->peer_addr().toIpPort() == es.conn_->peerAddress().toIpPort())
		{
			EventLoop::getEventLoopOfCurrentThread()->runInLoop(std::bind(&GameServer::Register2Master, this, master_session));
			break;
		}
		else if (!es.conn_->connected() && 
            master_session->connected() &&
			master_session->peer_addr().toIpPort() == es.conn_->peerAddress().toIpPort())
		{
			g_ms_nodes.erase(ms_node->node_id());
			break;
		}
	}
}

void GameServer::receive(const common::OnBeConnectedEvent& es)
{
    auto& conn = es.conn_;
	if (conn->connected())
	{
		auto e = reg.create();
		reg.emplace<RpcServerConnection>(e, RpcServerConnection{ conn });
	}
    else
    {
		auto& peer_addr = conn->peerAddress();
		for (auto e : reg.view<RpcServerConnection>())
		{
			auto& local_addr = reg.get<RpcServerConnection>(e).conn_->peerAddress();
			if (local_addr.toIpPort() != peer_addr.toIpPort())
			{
				continue;
			}
			auto gatenode = reg.try_get<GateNodePtr>(e);//Èç¹ûÊÇgate
			if (nullptr != gatenode && (*gatenode)->node_info_.node_type() == GATEWAY_NODE_TYPE)
			{
                g_gate_nodes.erase((*gatenode)->node_info_.node_id());
			}
			reg.destroy(e);
			break;
		}
    }
}

void GameServer::InitGlobalEntities()
{
    reg.emplace<SceneMapComp>(global_entity());
}

void GameServer::ConnectRegion()
{
    region_session_->subscribe<RegisterStubEvent>(g2rg_stub_);
    region_session_->registerService(&gs_service_impl_);
    region_session_->subscribe<OnConnected2ServerEvent>(*this);
    region_session_->connect();
}
