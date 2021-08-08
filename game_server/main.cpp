#include "muduo/net/EventLoop.h"
#include "src/game_server.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    EventLoop loop;
    game::GameServer server(&loop);
    g_game_server = &server;
    server.LoadConfig();
    server.InitNetwork();
    loop.loop();
    return 0;
}