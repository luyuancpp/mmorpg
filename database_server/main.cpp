#ifdef __linux__
#include <unistd.h>
#endif//__linux__

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/protorpc/RpcServer.h"

#include "database_server.h"


using namespace muduo;
using namespace muduo::net;
using namespace database;
using namespace common;

int32_t main(int argc, char* argv[])
{
    ConnectionParameters query_database_param{ "127.0.0.1", "root" , "luyuan616586", "game" , 3306 };
    EventLoop loop;
    InetAddress listenAddr("127.0.0.1", 2003);
    DatabaseServer server(&loop, listenAddr, query_database_param);
    server.Start();
    loop.loop();
    return 0;
}

