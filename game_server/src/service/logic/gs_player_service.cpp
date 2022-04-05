#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/scene_client_player.pb.h"
#include "src/service/logic/gsscene_client_player.h"
#include "logic_proto/team_client_player.pb.h"
#include "src/service/logic/gsteam_client_player.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/service/logic/gsteam_server_player.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/service/logic/gsscene_server_player.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
std::unordered_set<std::string> g_open_player_services;
class clientplayerPlayerSceneServiceImpl : public clientplayer::PlayerSceneService{};
class clientplayerPlayerTeamServiceImpl : public clientplayer::PlayerTeamService{};
class serverplayerPlayerTeamServiceImpl : public serverplayer::PlayerTeamService{};
class serverplayerServerPlayerSceneServiceImpl : public serverplayer::ServerPlayerSceneService{};
void InitPlayerServcie()
{
    g_player_services.emplace("clientplayer.PlayerSceneService", std::make_unique<clientplayer::PlayerPlayerSceneServiceImpl>(new clientplayerPlayerSceneServiceImpl));
    g_player_services.emplace("clientplayer.PlayerTeamService", std::make_unique<clientplayer::PlayerPlayerTeamServiceImpl>(new clientplayerPlayerTeamServiceImpl));
    g_player_services.emplace("serverplayer.PlayerTeamService", std::make_unique<serverplayer::PlayerPlayerTeamServiceImpl>(new serverplayerPlayerTeamServiceImpl));
    g_player_services.emplace("serverplayer.ServerPlayerSceneService", std::make_unique<serverplayer::PlayerServerPlayerSceneServiceImpl>(new serverplayerServerPlayerSceneServiceImpl));
    g_open_player_services.emplace("clientplayer.PlayerSceneService");
    g_open_player_services.emplace("clientplayer.PlayerTeamService");
}
