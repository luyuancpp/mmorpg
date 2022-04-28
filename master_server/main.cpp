#include "muduo/net/EventLoop.h"

#include "src/master_server.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    //other global value
    EventLoop loop;
    MasterServer server(&loop);
    server.Init();
    loop.loop();
    return 0;
}