#include "src/database_server.h"

using namespace muduo;
using namespace muduo::net;

int32_t main(int argc, char* argv[])
{
    EventLoop loop;
    DatabaseServer server(&loop);
    g_db_server = &server;
    server.Init();
    loop.loop();
    return 0;
}

