#include "muduo/net/EventLoop.h"

#include "src/login_server.h"

int main(int argc, char* argv[])
{
    EventLoop loop;
    login::LoginServer server(&loop);
    server.Init();
    loop.loop();
    return 0;
}