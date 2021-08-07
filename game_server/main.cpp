#include "muduo/net/EventLoop.h"
#include "src/game_server.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    EventLoop loop;
    game::GameServer server(&loop);
    server.LoadConfig();
    server.InitNet();
    server.Register2Master();
    loop.loop();
    return 0;
}