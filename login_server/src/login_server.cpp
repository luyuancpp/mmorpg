#include "login_server.h"

#include "src/game_config/deploy_json.h"
#include "src/server_common/deploy_rpcclient.h"
#include "src/server_common/rpc_connection_event.h"
#include "src/server_common/server_type_id.h"

#include "common.pb.h"

namespace login
{
LoginServer::LoginServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<common::RedisClient>()),
      impl_( l2ms_login_stub_, l2db_login_stub_)
{
}

void LoginServer::LoadConfig()
{
    common::GameConfig::GetSingleton().Load("game.json");
    common::DeployConfig::GetSingleton().Load("deploy.json");
}

void LoginServer::ConnectDeploy()
{
    const auto& deploy_info = common::DeployConfig::GetSingleton().deploy_param();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<common::RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->subscribe<common::RegisterStubES>(deploy_stub_);
    deploy_rpc_client_->subscribe<common::RpcClientConnectionES>(*this);
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
    auto& databaseinfo = cp->s_resp_->info(common::kServerDatabase);
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    db_rpc_client_ = std::make_unique<common::RpcClient>(loop_, database_addr);
    db_rpc_client_->connect();
    db_rpc_client_->subscribe<common::RegisterStubES>(l2db_login_stub_);

    auto& masterinfo = cp->s_resp_->info(common::kServerMaster);
    InetAddress master_addr(masterinfo.ip(), masterinfo.port());
    master_rpc_client_ = std::make_unique<common::RpcClient>(loop_, master_addr);
    master_rpc_client_->connect();
    master_rpc_client_->subscribe<common::RegisterStubES>(l2ms_login_stub_);
    
    auto& redisinfo = cp->s_resp_->redis_info();
    redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);
 
    auto& myinfo = cp->s_resp_->info(common::kServerLogin);

    InetAddress login_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, login_addr);
   
    Start();
}

void LoginServer::receive(const common::RpcClientConnectionES& es)
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
    cp->s_reqst_.set_group(common::GameConfig::GetSingleton().config_info().group_id());
    deploy_stub_.CallMethod(
        &LoginServer::StartServer,
        cp,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}

}


