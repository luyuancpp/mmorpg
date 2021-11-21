#include "muduo/net/EventLoop.h"

#include "src/master_server.h"
#include "src/scene/sceces.h"

using namespace muduo;
using namespace muduo::net;
using namespace master;

int main(int argc, char* argv[])
{
    EventLoop loop;
    MasterServer server(&loop);
    g_master_server = &server;

    ScenesManager scene_manager;
    g_scene_manager = &scene_manager;

    server.Init();
    server.ConnectDeploy();
    loop.loop();
    return 0;
}