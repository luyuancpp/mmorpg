#include "database_server.h"

#include "src/game_config/deploy_json.h"
#include "src/network/rpc_connection_event.h"
#include "src/pb/pbc/service_method/deploy_servicemethod.h"
#include "src/service/common_proto_replied/replied_dispathcer.h"

#include "mysql_database_table.pb.h"
#include "deploy_service.pb.h"

DatabaseServer* g_database_node{nullptr};

DatabaseServer::DatabaseServer(muduo::net::EventLoop* loop)
    : loop_(loop),
        database_(std::make_shared<MysqlClientPtr::element_type>()),
        redis_(std::make_shared<PbSyncRedisClientPtr::element_type>()){}

void DatabaseServer::Init()
{
    GameConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");

    InitRepliedCallback();
    ConnectDeploy();
}

void DatabaseServer::ConnectDeploy()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_session_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_session_->subscribe<OnConnected2ServerEvent>(*this); 
    deploy_session_->connect();
}

void DatabaseServer::Start()
{
    database_->AddTable(account_database::default_instance());
    database_->AddTable(player_database::default_instance());
    static const uint64_t begin_guid = 10000000000;
    database_->set_auto_increment(player_database::default_instance(),
        GameConfig::GetSingleton().config_info().group_id() * begin_guid);
    database_->Init();

    server_->registerService(&impl_);
    server_->start();
}

void DatabaseServer::StartServer(const ::servers_info_data& info)
{
    auto& redisinfo = info.redis_info();
    auto& myinfo = info.database_info();
    InetAddress listenAddr(myinfo.ip(), myinfo.port());
    redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);
    database_->Connect(myinfo);
    server_ = std::make_shared<RpcServerPtr::element_type>(loop_, listenAddr);
    Start();
}

void DatabaseServer::receive(const OnConnected2ServerEvent& es)
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
    deploy_session_->CallMethod(DeployServiceServerInfo, &rq);
}



