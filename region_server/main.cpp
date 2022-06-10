#include "muduo/net/EventLoop.h"

#include "src/region_server.h"
#include "src/network/gs_node.h"
#include "src/network/ms_node.h"

using namespace muduo;
using namespace muduo::net;
using namespace region;

int32_t main()
{
    //other global value
    MsNodes ms_nodes;
    g_ms_nodes = &ms_nodes;

    GsNodes gs_node;
    g_gs_nodes = &gs_node;

    EventLoop loop;
    RegionServer server(&loop);
    server.Init();
    loop.loop();
}
