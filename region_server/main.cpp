#include "muduo/net/EventLoop.h"

#include "src/region_server.h"

using namespace muduo;
using namespace muduo::net;
using namespace region;

int32_t main()
{
    EventLoop loop;
    RegionServer server(&loop);
    g_region_server = &server;
    server.Init();
    server.ConnectDeploy();
    loop.loop();
}
