#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include "src/game_config/deploy_json.h"
#include "src/deploy_server.h"

using namespace muduo::net;

int32_t main(int argc, char* argv[])
{
    DeployConfig::GetSingleton().Load("deploy.json");
    auto deploy_server_info = DeployConfig::GetSingleton().deploy_info();
    EventLoop loop;
    InetAddress listenAddr(deploy_server_info.ip(), deploy_server_info.port());
    DeployServer server(&loop, listenAddr);
    server.Start();
    loop.loop();
    return 0;
}

