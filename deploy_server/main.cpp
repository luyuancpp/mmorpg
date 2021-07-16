#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include "src/game_config/game_config.h"
#include "deploy_server.h"

#include "src/service/service.h"

using namespace muduo::net;

int32_t main(int argc, char* argv[])
{
    common::DeployConfig::GetSingleton().Load("deploy.json");
    auto deploy_server_info = common::DeployConfig::GetSingleton().deploy_param();
    EventLoop loop;
    InetAddress listenAddr(deploy_server_info.host_name(), deploy_server_info.port());
    deploy_server::DeployServer server(&loop, listenAddr);
    deploy::DeployServiceImpl impl;
    impl.set_player_mysql_client(server.player_mysql_client());
    server.RegisterService(&impl);
    server.Start();
    loop.loop();
    return 0;
}

