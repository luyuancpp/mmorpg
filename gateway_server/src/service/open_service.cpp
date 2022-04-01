#include <unordered_set>
#include "logic_proto/player/team_ms.pb.h"
#include "logic_proto/player/team_gs.pb.h"
#include "logic_proto/player/scene_ms.pb.h"
#include "logic_proto/player/scene_gs.pb.h"
namespace gateway
{
std::unordered_set<std::string> g_open_player_services;
void OpenPlayerServcie()
{
    g_open_player_services.emplace("msplayerservice.PlayerTeamService");
    g_open_player_services.emplace("playerservice.PlayerTeamService");
    g_open_player_services.emplace("msplayerservice.PlayerSceneService");
    g_open_player_services.emplace("playerservice.PlayerSceneService");
}
}//namespace gateway
