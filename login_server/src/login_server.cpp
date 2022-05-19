#include "login_server.h"

#include "src/game_config/deploy_json.h"
#include "src/network/deploy_rpcclient.h"
#include "src/network/rpc_connection_event.h"

#include "common.pb.h"

using namespace common;

login::LoginServer* g_login_server = nullptr;

namespace login
{
LoginServer::LoginServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<MessageSyncRedisClient>()),
      impl_( l2ms_login_stub_, l2db_login_stub_)
{
}

void LoginServer::Init()
{
    g_login_server = this;
    GameConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");

    ConnectDeploy();
}

void LoginServer::ConnectDeploy()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->subscribe<RegisterStubEvent>(deploy_stub_);
    deploy_rpc_client_->subscribe<OnConnected2ServerEvent>(*this);
    deploy_rpc_client_->connect();
}

void LoginServer::Start()
{
    server_->registerService(&impl_);
    impl_.set_redis_client(redis_client());
    server_->start();
}

void LoginServer::StartServer(ServerInfoRpcRC cp)
{
    auto& info = cp->s_rp_->info();
    auto& databaseinfo = info.database_info();
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    db_rpc_client_ = std::make_unique<RpcClient>(loop_, database_addr);
    db_rpc_client_->connect();
    db_rpc_client_->subscribe<RegisterStubEvent>(l2db_login_stub_);

    auto& masterinfo = info.master_info();
    InetAddress master_addr(masterinfo.ip(), masterinfo.port());
    master_rpc_client_ = std::make_unique<RpcClient>(loop_, master_addr);
    master_rpc_client_->connect();
    master_rpc_client_->subscribe<RegisterStubEvent>(l2ms_login_stub_);
    
    auto& redisinfo = info.redis_info();
    redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);
 
    auto& myinfo = info.login_info();

    InetAddress login_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, login_addr);
   
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
    ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
    cp->s_rq_.set_group(GameConfig::GetSingleton().config_info().group_id());
    deploy_stub_.CallMethod(
        &LoginServer::StartServer,
        cp,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}

}


