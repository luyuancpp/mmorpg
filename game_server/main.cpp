#include "muduo/net/EventLoop.h"

#include "src/game_logic/player/player_list.h"
#include "src/game_server.h"
#include "src/game_logic/thread_local/game_registry.h"
#include "src/network/node_info.h"
#include "src/network/gate_node.h"
#include "src/network/controller_node.h"

using namespace muduo;
using namespace muduo::net;

int main(int argc, char* argv[])
{
    // global value
    ControllerNodes controller_node;
    g_controller_nodes = &controller_node;

    GateNodes gate_nodes;
    g_gate_nodes = &gate_nodes;


	GateSessionList gate_sessions;
    g_gate_sessions = &gate_sessions;

    PlayerList players;
    g_players = &players;

    EventLoop loop;
    GameServer server(&loop);
    server.Init();
    loop.loop();
    return 0;
}