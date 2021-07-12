#include "master_server.h"

#include "src/database/rpcclient/database_rpcclient.h"
#include "src/login/service.h"

#include "l2ms.pb.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    int nThreads = argc > 1 ? atoi(argv[1]) : 1;
    EventLoop loop;
    InetAddress listen_addr("127.0.0.1", 2004);
    InetAddress database_addr("127.0.0.1", 2003);

    master::DbRpcClient::Connect(&loop, database_addr);

    l2ms::LoginServiceImpl impl;
    master::MasterServer server(&loop, listen_addr);
    server.RegisterService(&impl);
    server.Start();
    loop.loop();
    return 0;
}