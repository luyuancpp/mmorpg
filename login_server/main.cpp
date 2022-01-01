#include "muduo/net/EventLoop.h"

#include "src/login_server.h"

using namespace login;

int main(int argc, char* argv[])
{
    EventLoop loop;
    LoginServer server(&loop);
    g_login_server = &server;
    server.LoadConfig();
    server.ConnectDeploy();
    loop.loop();
    return 0;
}