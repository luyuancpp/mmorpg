#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/service/logic/team_server_player.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/service/logic/scene_server_player.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
std::unordered_set<std::string> g_open_player_services;
class PlayerTeamServiceImpl : public serverplayer::PlayerTeamService{};
class PlayerSceneServiceImpl : public serverplayer::PlayerSceneService{};
void InitPlayerServcie()
{
    g_player_services.emplace("serverplayer.PlayerTeamService", std::make_unique<serverplayer::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_player_services.emplace("serverplayer.PlayerSceneService", std::make_unique<serverplayer::PlayerPlayerSceneServiceImpl>(new PlayerSceneServiceImpl));
}
