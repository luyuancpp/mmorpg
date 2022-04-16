#include "muduo/net/EventLoop.h"

#include "src/game_server.h"
#include "src/game_logic/game_registry.h"
#include "src/network/node_info.h"

using namespace muduo;
using namespace muduo::net;
using namespace common;

int main(int argc, char* argv[])
{
    global_entity() = reg.create();
    if (argc > 1 && atoi(argv[1]) == kRoomServer)
    {
        reg.emplace<eServerType>(global_entity(), kRoomServer);
    }
    else
    {
        reg.emplace<eServerType>(global_entity(), kMainSceneServer);
    }

    EventLoop loop;
    GameServer server(&loop);
    g_gs = &server;
    server.Init();
    server.InitNetwork();
    loop.loop();
    return 0;
}