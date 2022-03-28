#include "game_server.h"

#include "muduo/base/Logging.h"
#include "muduo/base/CrossPlatformAdapterFunction.h"

#include "src/game_config/all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/region_config.h"

#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/game_logic/game_registry.h"
#include "src/service/player_service.h"
#include "src/service/replied_ms2gs.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/node_info.h"
#include "src/pb/pbc/msgmap.h"

using namespace common;

game::GameServer* g_gs = nullptr;

namespace game
{
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
    deploy_session_->subscribe<OnClientConnectedEvent>(*this);
    deploy_session_->connect();
}

void GameServer::ServerInfo(ServerInfoRpcRC cp)
{
    auto& resp = cp->s_rp_;
    //LOG_INFO << resp->DebugString().c_str();
    auto& info = cp->s_rp_->info();
   
    InitRoomMasters(resp);

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
}

void GameServer::StartGSDeployReplied(StartGSRpcRC cp)
{
    //uint32_t snid = server_info_.id() - deploy_server::kGameSnowflakeIdReduceParam;//snowflake id 
    ConnectMaster();
    ConnectRegion();

    server_deploy_ = cp->s_rp_->my_info();
    InetAddress node_addr(server_deploy_.ip(), server_deploy_.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, node_addr);
    server_->registerService(&gw2gs_impl_);
    server_->start();   
}

void GameServer::Register2Master(MasterSessionPtr& master_rpc_client)
{
    ms2gs::RepliedMs2g::StartGameMasterRpcRC scp(std::make_shared<ms2gs::RepliedMs2g::StartGameMasterRpcClosure>());
    auto& master_local_addr = master_rpc_client->local_addr();
    gs2ms::StartGSRequest& request = scp->s_rq_;
    auto session_info = request.mutable_rpc_client();
    auto node_info = request.mutable_rpc_server();
    session_info->set_ip(master_local_addr.toIp());
    session_info->set_port(master_local_addr.port());
    node_info->set_ip(server_deploy_.ip());
    node_info->set_port(server_deploy_.port());
    request.set_server_type(reg.get<eServerType>(game::global_entity()));
    request.set_gs_node_id(server_deploy_.id());
    request.set_master_server_addr(uint64_t(master_rpc_client.get()));
    g2ms_stub_.CallMethod(
        &ms2gs::RepliedMs2g::StartGSMasterReplied,
        scp,
        &ms2gs::RepliedMs2g::GetSingleton(),
        &gs2ms::G2msService_Stub::StartGS);
}

void GameServer::receive(const OnClientConnectedEvent& es)
{
    if (!es.conn_->connected())
    {
        return;
    }

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
                if (reg.get<eServerType>(game::global_entity()) == kMainServer)
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

    for (auto e : reg.view<MasterSessionPtr>())
    {
        auto& master_session = reg.get<MasterSessionPtr>(e);
        if (master_session->connected() &&
            master_session->peer_addr().toIpPort() == es.conn_->peerAddress().toIpPort())
        {
            EventLoop::getEventLoopOfCurrentThread()->runInLoop(std::bind(&GameServer::Register2Master, this, master_session));
            break;
        }
    }
}

void GameServer::InitGlobalEntities()
{
    reg.emplace<SceneMapComp>(global_entity());
}

void GameServer::InitRoomMasters(const deploy::ServerInfoResponse* resp)
{
    auto e = reg.create();
    auto& regionmaster = resp->region_masters();
    if (regionmaster.masters_size() <= 0)
    {
        auto& masterinfo = resp->info().master_info();
        InetAddress master_addr(masterinfo.ip(), masterinfo.port());
        reg.emplace<MasterSessionPtr>(e, std::make_shared<RpcClient>(loop_, master_addr));
        return;
    }
    for (int32_t i = 0; i < regionmaster.masters_size(); ++i)
    {
        auto& masterinfo = regionmaster.masters(i);
        InetAddress master_addr(masterinfo.ip(), masterinfo.port());
        reg.emplace<MasterSessionPtr>(e, std::make_shared<RpcClient>(loop_, master_addr));
    }
}

void GameServer::ConnectMaster()
{
    for (auto e : reg.view<MasterSessionPtr>())
    {
        auto& master_rpc_client = reg.get<MasterSessionPtr>(e);
        master_rpc_client->subscribe<RegisterStubEvent>(g2ms_stub_);
        master_rpc_client->registerService(&ms2g_impl_);
        master_rpc_client->subscribe<OnClientConnectedEvent>(*this);
        master_rpc_client->connect();
    }    
}

void GameServer::ConnectRegion()
{
    region_session_->subscribe<RegisterStubEvent>(g2rg_stub_);
    region_session_->registerService(&rg2g_impl_);
    region_session_->subscribe<OnClientConnectedEvent>(*this);
    region_session_->connect();
}

}//namespace game
