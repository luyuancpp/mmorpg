#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/team_ms.pb.h"

#include "logic_proto/scene_ms.pb.h"

#include "logic_proto/team_gs.pb.h"
#include "team_gs_player.h"
#include "logic_proto/scene_gs.pb.h"
#include "scene_gs_player.h"
namespace game
{
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
std::unordered_set<std::string> g_open_player_services;

class PlayerTeamServiceImpl : public gsplayerservice::PlayerTeamService{};
class PlayerSceneServiceImpl : public gsplayerservice::PlayerSceneService{};
void InitPlayerServcie()
{
    g_open_player_services.emplace("gsplayerservice.PlayerTeamService");
    g_player_services.emplace("gsplayerservice.PlayerTeamService", std::make_unique<gsplayerservice::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_open_player_services.emplace("gsplayerservice.PlayerSceneService");
    g_player_services.emplace("gsplayerservice.PlayerSceneService", std::make_unique<gsplayerservice::PlayerPlayerSceneServiceImpl>(new PlayerSceneServiceImpl));
}
}//namespace game
