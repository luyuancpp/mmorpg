#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include "src/game_config/deploy_json.h"
#include "src/deploy_server.h"

#include "src/service/service.h"

using namespace muduo::net;
using namespace common;

int32_t main(int argc, char* argv[])
{
    DeployConfig::GetSingleton().Load("deploy.json");
    auto deploy_server_info = DeployConfig::GetSingleton().deploy_param();
    EventLoop loop;
    InetAddress listenAddr(deploy_server_info.ip(), deploy_server_info.port());
    deploy::DeployServer server(&loop, listenAddr);
    g_deploy_server = &server;
    deploy::DeployServiceImpl impl;
    impl.set_player_mysql_client(server.player_mysql_client());
    server.RegisterService(&impl);
    server.Start();
    loop.loop();
    return 0;
}

