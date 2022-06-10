#include "muduo/net/EventLoop.h"

#include "src/master_server.h"
#include "src/comp/player_list.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    //global value
    PlayerList player_list;
    g_player_list = &player_list;

    EventLoop loop;
    MasterServer server(&loop);
    server.Init();
    loop.loop();
    return 0;
}