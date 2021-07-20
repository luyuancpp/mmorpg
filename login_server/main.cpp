#include "muduo/net/EventLoop.h"

#include "src/login_server.h"

using namespace login;

int main(int argc, char* argv[])
{
    EventLoop loop;
    LoginServer server(&loop);
    server.LoadConfig();
    server.ConnectDeploy();
    loop.loop();
    return 0;
}