#include "muduo/net/EventLoop.h"

#include "src/game_server.h"

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