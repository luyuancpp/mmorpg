#include "game_server.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    int nThreads = argc > 1 ? atoi(argv[1]) : 1;
    EventLoop loop;
    InetAddress listen_addr("127.0.0.1", 2005);
    InetAddress database_addr("127.0.0.1", 2003);

    database.Connect(&loop, database_addr);

    RpcServer server(&loop, listen_addr);
    server.setThreadNum(nThreads);
    server.start();
    loop.loop();
    return 0;
}