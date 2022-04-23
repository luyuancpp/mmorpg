#include "src/network/deploy_rpcclient.h"

#include "src/database_server.h"

using namespace muduo;
using namespace muduo::net;
using namespace database;

int32_t main(int argc, char* argv[])
{
    EventLoop loop;
    DatabaseServer server(&loop);
    server.Init();
    loop.loop();
    return 0;
}

