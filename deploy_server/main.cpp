#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"

#include "deploy_server.h"

using namespace muduo::net;

int32_t main(int argc, char* argv[])
{
    EventLoop loop;
    InetAddress listenAddr("127.0.0.1", 1000);
    deploy_server::DeployServer server(&loop, listenAddr);
    server.Start();
    loop.loop();
    return 0;
}

