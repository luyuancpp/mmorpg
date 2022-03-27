#include <unordered_set>
#include "logic_proto/player_team.pb.h"
#include "logic_proto/player_scene.pb.h"
#include "logic_proto/ms2gs_scene.pb.h"
namespace gateway
{
std::unordered_set<std::string> g_open_player_services;
void OpenPlayerServcie()
{
    g_open_player_services.emplace("playerservice.PlayerTeamService");
    g_open_player_services.emplace("playerservice.PlayerSceneService");
}
}//namespace gateway
