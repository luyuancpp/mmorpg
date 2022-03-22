#include "src/gateway_server.h"
#include "src/gate_player/gate_player_list.h"

using namespace gateway;

int main(int argc, char* argv[])
{
    ClientSessions gate_clients;
    g_client_sessions_ = &gate_clients;
    EventLoop loop;
    GatewayServer server(&loop);
    g_gateway_server = &server;
    server.Init();
    loop.loop();
    return 0;
}