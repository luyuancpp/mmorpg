#include "src/gateway_server.h"

using namespace gateway;

int main(int argc, char* argv[])
{
    EventLoop loop;
    GatewayServer server(&loop);
    server.LoadConfig();
    server.ConnectDeploy();
    loop.loop();
    return 0;
}