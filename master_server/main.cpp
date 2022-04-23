#include "muduo/net/EventLoop.h"

#include "src/master_server.h"
#include "src/game_logic/scene/scene.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    //other global value
	ScenesSystem scene_scene;
	g_scene_sys = &scene_scene;

    EventLoop loop;
    MasterServer server(&loop);
    server.Init();
    loop.loop();
    return 0;
}