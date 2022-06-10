#include "muduo/net/EventLoop.h"

#include "src/game_server.h"
#include "src/game_logic/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/gate_node.h"
#include "src/network/ms_node.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    MsNodes ms_node;
    g_ms_nodes = &ms_node;

    GateNodes gate_nodes;
    g_gate_nodes = &gate_nodes;

    EventLoop loop;
    GameServer server(&loop);
    server.Init();
    loop.loop();
    return 0;
}