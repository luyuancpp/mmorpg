#include "gateway_server.h"

using namespace gateway;

int main(int argc, char* argv[])
{
    EventLoop loop;

    InetAddress login_server_addr("127.0.0.1", 2001);
    InetAddress server_addr("127.0.0.1", 2000);
    GatewayServer server(&loop, server_addr);
    LoginClient::GetSingleton() = std::make_unique<LoginClient::StubType>(&loop, login_server_addr);
    LoginClient::GetSingleton()->connect();
    server.Start();
    loop.loop();
    return 0;
}