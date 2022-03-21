#include "muduo/net/EventLoop.h"

#include "src/game_server.h"
#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/game_registry.h"
#include "src/server_common/node_info.h"

using namespace muduo;
using namespace muduo::net;
using namespace common;

int main(int argc, char* argv[])
{
    game::global_entity() = reg.create();
    if (argc > 1 && atoi(argv[1]) == kRoomServer)
    {
        reg.emplace<eServerType>(game::global_entity(), kRoomServer);
    }
    else
    {
        reg.emplace<eServerType>(game::global_entity(), kMainServer);
    }

    EventLoop loop;
    game::GameServer server(&loop);
    g_gs = &server;
    server.Init();
    server.InitNetwork();
    loop.loop();
    return 0;
}