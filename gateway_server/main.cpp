#include "src/gateway_server.h"
#include "src/gate_player/gate_player_list.h"

using namespace gateway;

int main(int argc, char* argv[])
{
    GateClientList gate_clients;
    g_gate_clients_ = &gate_clients;
    EventLoop loop;
    GatewayServer server(&loop);
    server.LoadConfig();
    server.ConnectDeploy();
    loop.loop();
    return 0;
}