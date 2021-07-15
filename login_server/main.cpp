#include "login_server.h"

#include "src/game_config/game_config.h"
#include "src/net/deploy/rpcclient/deploy_rpcclient.h"

using namespace login;

int main(int argc, char* argv[])
{
    common::GameConfig::GetSingleton().Load("game.json");
    const auto& deploy_info = common::GameConfig::GetSingleton().deploy_server();
;
    EventLoop loop;
    InetAddress deploy_addr(deploy_info.host_name(), deploy_info.port());
    deploy::DeployRpcClient::Connect(&loop, deploy_addr);
    deploy::ServerInfoRpcStub::GetSingleton();
    
    LoginServer server(&loop);
    loop.loop();
    return 0;
}