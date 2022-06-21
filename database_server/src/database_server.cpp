#include "database_server.h"

#include "src/game_config/deploy_json.h"
#include "src/network/deploy_rpcclient.h"
#include "src/network/rpc_connection_event.h"

#include "mysql_database_table.pb.h"

using namespace common;

DatabaseServer::DatabaseServer(muduo::net::EventLoop* loop)
    : loop_(loop),
        database_(std::make_shared<MysqlClientPtr::element_type>()),
        redis_(std::make_shared<PbSyncRedisClientPtr::element_type>()){}

void DatabaseServer::Init()
{
    GameConfig::GetSingleton().Load("game.json");
    DeployConfig::GetSingleton().Load("deploy.json");

    ConnectDeploy();
}

void DatabaseServer::ConnectDeploy()
{
    const auto& deploy_info = DeployConfig::GetSingleton().deploy_info();
    InetAddress deploy_addr(deploy_info.ip(), deploy_info.port());
    deploy_rpc_client_ = std::make_unique<RpcClient>(loop_, deploy_addr);
    deploy_rpc_client_->subscribe<RegisterStubEvent>(deploy_stub_);
    deploy_rpc_client_->subscribe<OnConnected2ServerEvent>(*this); 
    deploy_rpc_client_->connect();
}

void DatabaseServer::Start()
{
    database_->AddTable(account_database::default_instance());
    database_->AddTable(player_database::default_instance());
    static const uint64_t begin_guid = 10000000000;
    database_->set_auto_increment(player_database::default_instance(),
        GameConfig::GetSingleton().config_info().group_id() * begin_guid);
    database_->Init();

    impl_.set_player_mysql_client(player_mysql_client());
    impl_.set_redis_client(redis_client());
    server_->registerService(&impl_);
    server_->start();
}

void DatabaseServer::StartServer(ServerInfoRpcRpc replied)
{
    auto& info = replied->s_rp_->info();
    auto& redisinfo = info.redis_info();
    auto& myinfo = info.database_info();
    InetAddress listenAddr(myinfo.ip(), myinfo.port());
    redis_->Connect(redisinfo.ip(), redisinfo.port(), 1, 1);
    database_->Connect(myinfo);
    //LOG_INFO << myinfo.DebugString().c_str();
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
    ServerInfoRpcRpc rpc(std::make_shared<ServerInfoRpcRpc::element_type>());
    rpc->s_rq_.set_group(GameConfig::GetSingleton().config_info().group_id());
    deploy_stub_.CallMethod(
        &DatabaseServer::StartServer,
        rpc,
        this,
        &deploy::DeployService_Stub::ServerInfo);
}



