#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/player/team_gs.pb.h"
#include "team_gs_player.h"
#include "logic_proto/player/scene_gs.pb.h"
#include "scene_gs_player.h"
#include "logic_proto/player/team_ms.pb.h"
#include "team_ms_player.h"
#include "logic_proto/player/scene_ms.pb.h"
#include "scene_ms_player.h"
namespace game
{
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
std::unordered_set<std::string> g_open_player_services;
class PlayerTeamServiceImpl : public playerservice::PlayerTeamService{};
class PlayerSceneServiceImpl : public playerservice::PlayerSceneService{};
class PlayerTeamServiceImpl : public msplayerservice::PlayerTeamService{};
class PlayerSceneServiceImpl : public msplayerservice::PlayerSceneService{};
void InitPlayerServcie()
{
    g_open_player_services.emplace("playerservice.PlayerTeamService");
    g_player_services.emplace("playerservice.PlayerTeamService", std::make_unique<playerservice::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_open_player_services.emplace("playerservice.PlayerSceneService");
    g_player_services.emplace("playerservice.PlayerSceneService", std::make_unique<playerservice::PlayerPlayerSceneServiceImpl>(new PlayerSceneServiceImpl));
    g_open_player_services.emplace("msplayerservice.PlayerTeamService");
    g_player_services.emplace("msplayerservice.PlayerTeamService", std::make_unique<msplayerservice::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_open_player_services.emplace("msplayerservice.PlayerSceneService");
    g_player_services.emplace("msplayerservice.PlayerSceneService", std::make_unique<msplayerservice::PlayerPlayerSceneServiceImpl>(new PlayerSceneServiceImpl));
}
}//namespace game
