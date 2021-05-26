#include "l2db.pb.h"

#ifdef __linux__
#include <unistd.h>
#endif//__linux__

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/protorpc/RpcServer.h"

#include "src/login/service.h"

using namespace muduo;
using namespace muduo::net;


int main(int argc, char* argv[])
{
    int nThreads = argc > 1 ? atoi(argv[1]) : 1;
    EventLoop loop;
    InetAddress listenAddr("127.0.0.1", 2003);
    l2db::LoginServiceImpl impl;
    RpcServer server(&loop, listenAddr);
    server.setThreadNum(nThreads);
    server.registerService(&impl);
    server.start();
    loop.loop();
}

