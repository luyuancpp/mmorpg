#include "muduo/net/EventLoop.h"

#include "src/region_server.h"

using namespace muduo;
using namespace muduo::net;
using namespace region;

int32_t main()
{
    //other global value

    EventLoop loop;
    RegionServer server(&loop);
    server.Init();
    loop.loop();
}
