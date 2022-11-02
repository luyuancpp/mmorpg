#include "muduo/net/EventLoop.h"

#include "src/region_server.h"
#include "src/network/gs_node.h"
#include "src/network/controller_node.h"

using namespace muduo;
using namespace muduo::net;

int32_t main()
{
    //other global value
    ControllerNodes controller_nodes;
    g_controller_nodes = &controller_nodes;

    GsNodes gs_node;
    g_gs_nodes = &gs_node;

    EventLoop loop;
    RegionServer server(&loop);
    server.Init();
    loop.loop();
}
