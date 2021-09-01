#include "muduo/net/EventLoop.h"

#include "src/master_server.h"

using namespace muduo;
using namespace muduo::net;
using namespace master;

int main(int argc, char* argv[])
{
    EventLoop loop;
    MasterServer server(&loop);
    g_master_server = &server;
    server.Init();
    server.ConnectDeploy();
    loop.loop();
    return 0;
}