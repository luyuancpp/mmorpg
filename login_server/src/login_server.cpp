#include "login_server.h"

#include "src/game_config/deploy_json.h"
#include "src/network/rpc_connection_event.h"
#include "src/pb/pbc/service_method/deploy_servicemethod.h"
#include "src/service/common_proto_replied/replied_dispathcer.h"

#include "common.pb.h"

LoginServer* g_login_node = nullptr;

LoginServer::LoginServer(muduo::net::EventLoop* loop)
    : loop_(loop),
      redis_(std::make_shared<PbSyncRedisClientPtr::element_type>()),
      impl_()
{
}

void LoginServer::Init()
{
    g_login_node = this;
    GameConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");

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
    server_->registerService(&impl_);
    server_->start();
}

void LoginServer::StartServer(const ::servers_info_data& info)
{
    auto& databaseinfo = info.database_info();
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    db_session_ = std::make_unique<RpcClient>(loop_, database_addr);
    db_session_->connect();

    auto& controller_node_info = info.controller_info();
    InetAddress controller_node_addr(controller_node_info.ip(), controller_node_info.port());
    controller_session_ = std::make_unique<RpcClient>(loop_, controller_node_addr);
    controller_session_->connect();
    
    auto& redisinfo = info.redis_info();
    redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);
 
    auto& myinfo = info.login_info();

    InetAddress login_addr(myinfo.ip(), myinfo.port());
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
    deploy_session_->CallMethod(DeployServiceServerInfoMethodDesc, &rq);
}
