#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include "src/config/database_config.h"
#include "src/deploy/rpcclient/deploy_rpcclient.h"
#include "src/game_rpc/game_rpc_server.h"
#include "src/login/service.h"
#include "src/deploy/deploy_stub/deploy_stub.h"

#include "database_server.h"

#include "l2db.pb.h"

using namespace muduo;
using namespace muduo::net;
using namespace database;
using namespace common;

int32_t main(int argc, char* argv[])
{
   
    
    database::DatabaseConfig::GetSingleton().Load("game.json");
    const auto& deploy_info =  database::DatabaseConfig::GetSingleton().deploy_server();

    EventLoop loop;
    InetAddress listenAddr(deploy_info.host_name(), deploy_info.port());
    database::DeployRpcClient::Connect(&loop, listenAddr);
    database::ServerInfoRpcStub::GetSingleton();
    database::DeployServiceImpl di;

    DatabaseServer server(&loop, listenAddr, query_database_param);
    l2db::LoginServiceImpl impl;
    impl.set_player_mysql_client(server.player_mysql_client());
    impl.set_redis_client(server.redis_client());
    server.RegisterService(&impl);
    server.Start();
    loop.loop();
    return 0;
}

