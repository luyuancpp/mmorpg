#include <unordered_set>
#include "logic_proto/team_ms.pb.h"
#include "logic_proto/team_gs.pb.h"
#include "logic_proto/scene_gs.pb.h"
#include "logic_proto/scene_ms.pb.h"
namespace gateway
{
std::unordered_set<std::string> g_open_player_services;
void OpenPlayerServcie()
{
    g_open_player_services.emplace("gsplayerservice.PlayerTeamService");
    g_open_player_services.emplace("gsplayerservice.PlayerSceneService");
}
}//namespace gateway
