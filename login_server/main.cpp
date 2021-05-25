#include "login_server.h"

#include "src/gateway/service/service.h"
#include "src/database/rpcclient/database_rpcclient.h"

int main(int argc, char* argv[])
{
    int nThreads = argc > 1 ? atoi(argv[1]) : 1;
    EventLoop loop;
    InetAddress listenAddr("127.0.0.1", 2001);
    InetAddress database_addr("127.0.0.1", 2003);

    database.Connect(&loop, database_addr);

    gw2l::LoginServiceImpl impl;
    RpcServer server(&loop, listenAddr);
    server.setThreadNum(nThreads);
    server.registerService(&impl);
    server.start();
    loop.loop();
    return 0;
}