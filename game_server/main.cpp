#include "muduo/net/EventLoop.h"

#include "src/game_server.h"
#include "src/game_logic/game_registry.h"
#include "src/network/node_info.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    EventLoop loop;
    GameServer server(&loop);
    server.Init();
    loop.loop();
    return 0;
}