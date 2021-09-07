#include "muduo/net/EventLoop.h"

#include "src/game_server.h"
#include "src/factories/server_global_entity.hpp"
#include "src/game_logic/enum/server_enum.h"
#include "src/game_logic/game_registry.h"

using namespace muduo;
using namespace muduo::net;

using common::reg;

int main(int argc, char* argv[])
{
    game::global_entity() = reg().create();
    if (argc > 1)
    {
        reg().emplace<common::eServerType>(game::global_entity(), common::kRoomServer);
    }
    else
    {
        reg().emplace<common::eServerType>(game::global_entity(), common::kMainServer);
    }

    EventLoop loop;
    game::GameServer server(&loop);
    g_game_server = &server;
    server.Init();
    server.InitNetwork();
    loop.loop();
    return 0;
}