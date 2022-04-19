#include "muduo/net/EventLoop.h"

#include "src/region_server.h"
#include "src/game_logic/scene/scene.h"

using namespace muduo;
using namespace muduo::net;
using namespace region;

int32_t main()
{
    EventLoop loop;
    RegionServer server(&loop);
    g_region_server = &server;
	ScenesSystem scene_scene;
	g_scene_sys = &scene_scene;
    server.Init();
    server.ConnectDeploy();
    loop.loop();
}
