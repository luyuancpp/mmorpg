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
    g_ms_node = &server;

    ScenesSystem scene_scene;
    g_scene_sys = &scene_scene;

    server.Init();
    server.ConnectDeploy();
    loop.loop();
    return 0;
}