#include "login_server.h"

#include "src/game_config/deploy_json.h"
#include "src/network/rpc_connection_event.h"
#include "src/network/node_info.h"
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
    node_info_.set_node_type(kLoginNode);
    node_info_.set_launch_time(Timestamp::now().microSecondsSinceEpoch());
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
    dababase_node_info_.set_node_id(databaseinfo.id());
    dababase_node_info_.set_node_type(kDatabaseNode);
    g_route2db_msg.mutable_node_info()->CopyFrom(dababase_node_info_);
    InetAddress database_addr(databaseinfo.ip(), databaseinfo.port());
    db_session_ = std::make_unique<RpcClient>(loop_, database_addr);
    db_session_->connect();

    auto& controller_info = info.controller_info();
    controller_node_info_.set_node_id(controller_info.id());
    controller_node_info_.set_node_type(kControllerNode);
    g_route2controller_msg.mutable_node_info()->CopyFrom(controller_node_info_);
    InetAddress controller_node_addr(controller_info.ip(), controller_info.port());
    controller_session_ = std::make_unique<RpcClient>(loop_, controller_node_addr);
    controller_session_->connect();
    
    auto& redisinfo = info.redis_info();
    redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);
 
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
    deploy_session_->CallMethod(DeployServiceServerInfoMethodDesc, &rq);
}
