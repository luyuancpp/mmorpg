#include "muduo/net/EventLoop.h"

#include "src/game/game_client.h"
#include "src/master_server.h"

using namespace muduo;
using namespace muduo::net;
using namespace master;

int main(int argc, char* argv[])
{
    GameClient::GetSingleton() = std::make_unique<GameClient::Type>();

    EventLoop loop;
    MasterServer server(&loop);
    server.LoadConfig();
    server.ConnectDeploy();
    loop.loop();
    return 0;
}