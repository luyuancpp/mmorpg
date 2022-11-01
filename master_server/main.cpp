#include "muduo/net/EventLoop.h"

#include "src/controller_server.h"
#include "src/comp/player_list.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    //global value
    PlayerList player_list;
    g_player_list = &player_list;

    EventLoop loop;
    ControllerServer server(&loop);
    server.Init();
    loop.loop();
    return 0;
}