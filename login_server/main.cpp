#include "login_server.h"

#include "muduo/net/protorpc/RpcServer.h"

#include "src/database/rpcclient/database_rpcclient.h"
#include "src/gateway/service.h"
#include "src/master/rpcclient/master_rpcclient.h"

int main(int argc, char* argv[])
{
    int nThreads = argc > 1 ? atoi(argv[1]) : 1;
    EventLoop loop;
    InetAddress listen_addr("127.0.0.1", 2001);
    InetAddress database_addr("127.0.0.1", 2003);
    InetAddress master_addr("127.0.0.1", 2004);

    login::DbRpcClient::s().Connect(&loop, database_addr);
    master.Connect(&loop, master_addr);

    gw2l::LoginServiceImpl impl;
    RpcServer server(&loop, listen_addr);
    server.setThreadNum(nThreads);
    server.registerService(&impl);
    server.start();
    loop.loop();
    return 0;
}