#include "src/gateway_server.h"
#include "src/network/gate_player_list.h"

int main(int argc, char* argv[])
{
	muduo::Logger::setLogLevel(muduo::Logger::ERROR_);
    //other global value
    ClientSessions gate_clients;
    g_client_sessions_ = &gate_clients;

    EventLoop loop;
    GatewayServer server(&loop);
    server.Init();
    loop.loop();
    return 0;
}