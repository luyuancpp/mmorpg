#include "src/server_common/deploy_rpcclient.h"

#include "database_server.h"

using namespace muduo;
using namespace muduo::net;
using namespace database;

int32_t main(int argc, char* argv[])
{
    EventLoop loop;
    DatabaseServer server(&loop);
    server.LoadConfig();
    server.ConnectDeploy();
    loop.loop();
    return 0;
}

