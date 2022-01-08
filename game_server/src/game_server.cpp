#include "game_server.h"

#include "muduo/base/Logging.h"
#include "muduo/base/CrossPlatformAdapterFunction.h"

#include "src/game_config/all_config.h"
#include "src/game_config/deploy_json.h"
#include "src/game_config/region_config.h"

#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/game_logic/enum/server_enum.h"
#include "src/game_logic/game_registry.h"
#include "src/master/replied_ms2g.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/deploy_variable.h"

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
    loadallconfig();
    InitGlobalEntities();
}

void GameServer::InitNetwork()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_param();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->subscribe<RegisterStubES>(deploy_stub_);
    deploy_rpc_client_->subscribe<RpcClientConnectionES>(*this);
    deploy_rpc_client_->connect();
}

void GameServer::ServerInfo(ServerInfoRpcRC cp)
{
    auto& resp = cp->s_resp_;
    //LOG_INFO << resp->DebugString().c_str();
    auto& info = cp->s_resp_->info();
   
    InitRoomMasters(resp);

    auto& regioninfo = info.regin_info();
    InetAddress region_addr(regioninfo.ip(), regioninfo.port());
   
    region_rpc_client_ = std::make_unique<RpcClient>(loop_, region_addr);
    
    StartGSRpcRC scp(std::make_shared<StartGSInfoRpcClosure>());
    scp->s_reqst_.set_group(GameConfig::GetSingleton().config_info().group_id());
    scp->s_reqst_.mutable_my_info()->set_ip(muduo::ProcessInfo::localip());
    scp->s_reqst_.mutable_my_info()->set_id(server_info_.id());
    scp->s_reqst_.mutable_rpc_client()->set_ip(deploy_rpc_client_->local_addr().toIp());
    scp->s_reqst_.mutable_rpc_client()->set_port(deploy_rpc_client_->local_addr().toPort());
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

    server_info_ = cp->s_resp_->my_info();
    InetAddress game_addr(server_info_.ip(), server_info_.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, game_addr);
    server_->start();   
}

void GameServer::Register2Master(MasterClientPtr& master_rpc_client)
{
    ms2g::RepliedMs2g::StartGameMasterRpcRC scp(std::make_shared<ms2g::RepliedMs2g::StartGameMasterRpcClosure>());
    auto& master_local_addr = master_rpc_client->local_addr();
    g2ms::StartGSRequest& request = scp->s_reqst_;
    auto rpc_client = request.mutable_rpc_client();
    auto rpc_server = request.mutable_rpc_server();
    rpc_client->set_ip(master_local_addr.toIp());
    rpc_client->set_port(master_local_addr.toPort());
    rpc_server->set_ip(server_info_.ip());
    rpc_server->set_port(server_info_.port());
    request.set_server_type(reg.get<eServerType>(game::global_entity()));
    request.set_node_id(server_info_.id());
    request.set_master_server_addr(uint64_t(master_rpc_client.get()));
    g2ms_stub_.CallMethod(
        &ms2g::RepliedMs2g::StartGSMasterReplied,
        scp,
        &ms2g::RepliedMs2g::GetSingleton(),
        &g2ms::G2msService_Stub::StartGS);
}

void GameServer::receive(const RpcClientConnectionES& es)
{
    if (!es.conn_->connected())
    {
        return;
    }

    if (deploy_rpc_client_->peer_addr().toIpPort() == es.conn_->peerAddress().toIpPort())
    {
        // started 
        if (nullptr != server_)
        {
            return;
        }

        ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
        if (reg.get<eServerType>(game::global_entity()) == kMainServer)
        {
            cp->s_reqst_.set_group(GameConfig::GetSingleton().config_info().group_id());
        }        
        cp->s_reqst_.set_region_id(RegionConfig::GetSingleton().config_info().region_id());
        deploy_stub_.CallMethod(
            &GameServer::ServerInfo,
            cp,
            this,
            &deploy::DeployService_Stub::ServerInfo);
    }

    for (auto e : reg.view<MasterClientPtr>())
    {
        auto& master_rpc_client = reg.get<MasterClientPtr>(e);
        if (master_rpc_client->connected() &&
            master_rpc_client->peer_addr().toIpPort() == es.conn_->peerAddress().toIpPort())
        {
            Register2Master(master_rpc_client);
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
        reg.emplace<MasterClientPtr>(e, std::make_shared<RpcClient>(loop_, master_addr));
        return;
    }
    for (int32_t i = 0; i < regionmaster.masters_size(); ++i)
    {
        auto& masterinfo = regionmaster.masters(i);
        InetAddress master_addr(masterinfo.ip(), masterinfo.port());
        reg.emplace<MasterClientPtr>(e, std::make_shared<RpcClient>(loop_, master_addr));
    }
}

void GameServer::ConnectMaster()
{
    for (auto e : reg.view<MasterClientPtr>())
    {
        auto& master_rpc_client = reg.get<MasterClientPtr>(e);
        master_rpc_client->subscribe<RegisterStubES>(g2ms_stub_);
        master_rpc_client->registerService(&ms2g_service_impl_);
        master_rpc_client->subscribe<RpcClientConnectionES>(*this);
        master_rpc_client->connect();
    }    
}

void GameServer::ConnectRegion()
{
    region_rpc_client_->subscribe<RegisterStubES>(g2rg_stub_);
    region_rpc_client_->registerService(&rg2g_service_impl_);
    region_rpc_client_->subscribe<RpcClientConnectionES>(*this);
    region_rpc_client_->connect();
}

}//namespace game
