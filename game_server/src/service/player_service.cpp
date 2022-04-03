#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/scene_client_player.pb.h"
#include "src/service/logic/scene_client_player.h"
#include "logic_proto/team_client_player.pb.h"
#include "src/service/logic/team_client_player.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/service/logic/team_server_player.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/service/logic/scene_server_player.h"
namespace game
{
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
std::unordered_set<std::string> g_open_player_services;
class PlayerSceneServiceImpl : public clientplayer::PlayerSceneService{};
class PlayerTeamServiceImpl : public clientplayer::PlayerTeamService{};
void InitPlayerServcie()
{
    g_player_services.emplace("clientplayer.PlayerSceneService", std::make_unique<clientplayer::PlayerPlayerSceneServiceImpl>(new PlayerSceneServiceImpl));
    g_player_services.emplace("clientplayer.PlayerTeamService", std::make_unique<clientplayer::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_player_services.emplace("serverplayer.PlayerTeamService", std::make_unique<serverplayer::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_player_services.emplace("serverplayer.PlayerSceneService", std::make_unique<serverplayer::PlayerPlayerSceneServiceImpl>(new PlayerSceneServiceImpl));
    g_open_player_services.emplace("clientplayer.PlayerSceneService");
    g_open_player_services.emplace("clientplayer.PlayerTeamService");
}
}//namespace game
