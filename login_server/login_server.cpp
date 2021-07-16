#include "login_server.h"

#include "src/database/rpcclient/database_rpcclient.h"
#include "src/game_config/game_config.h"
#include "src/net/deploy/rpcclient/deploy_rpcclient.h"
#include "src/rpc_closure_param/rpc_connection_event.h"
#include "src/server_type_id/server_type_id.h"

#include "common.pb.h"

namespace login
{
LoginServer::LoginServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<common::RedisClient>()),
      impl_( master_login_stub_, db_login_stub_)
{
}

void LoginServer::LoadConfig()
{
    common::GameConfig::GetSingleton().Load("game.json");
}

void LoginServer::ConnectDeploy()
{
    const auto& deploy_info = common::GameConfig::GetSingleton().deploy_server();
    InetAddress deploy_addr(deploy_info.host_name(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<common::RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->emp()->subscribe<common::RegisterStubEvent>(deploy_stub_);
    deploy_rpc_client_->emp()->subscribe<common::ConnectionEvent>(*this);
    deploy_rpc_client_->connect();
}

void LoginServer::Start()
{
    server_->registerService(&impl_);
    impl_.set_redis_client(redis_client());
    server_->start();
}

void LoginServer::receive(const common::ConnectionEvent& es)
{
    if (!es.conn_->connected())
    {
        return;
    }
    ServerInfoRpcRC cp(std::make_shared<ServerInfoRpcClosure>());
    cp->s_reqst_.set_group(1);
    deploy_stub_.CallMethod(
        &LoginServer::StartServer,
        cp,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}

void LoginServer::StartServer(ServerInfoRpcRC cp)
{
    auto& databaseinfo = cp->s_resp_->info(common::SERVER_DATABASE);
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    db_rpc_client_ = std::make_unique<common::RpcClient>(loop_, database_addr);
    db_rpc_client_->connect();
    db_rpc_client_->emp()->subscribe<common::RegisterStubEvent>(db_login_stub_);

    auto& masterinfo = cp->s_resp_->info(common::SERVER_MASTER);
    InetAddress master_addr(masterinfo.ip(), masterinfo.port());
    master_rpc_client_ = std::make_unique<common::RpcClient>(loop_, master_addr);
    master_rpc_client_->connect();
    master_rpc_client_->emp()->subscribe<common::RegisterStubEvent>(master_login_stub_);
    
    auto& redisinfo = cp->s_resp_->info(common::SERVER_REDIS);
    redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);
 
    auto& myinfo = cp->s_resp_->info(common::SERVER_LOGIN);
    InetAddress login_addr(myinfo.ip(), myinfo.port());
    server_ = std::make_shared<muduo::net::RpcServer>(loop_, login_addr);
   
    Start();
}

}


