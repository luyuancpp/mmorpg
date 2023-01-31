#include "muduo/net/EventLoop.h"

#include "src/lobby_server.h"

using namespace muduo;
using namespace muduo::net;

int32_t main()
{

    EventLoop loop;
    LobbyServer server(&loop);
    server.Init();
    loop.loop();
}
