#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/scene_ms.pb.h"
#include "src/service/logic_proto/logic/scene_ms.h"
#include "logic_proto/team_ms.pb.h"
#include "src/service/logic_proto/logic/team_ms.h"
#include "logic_proto/scene_client_player.pb.h"
#include "src/service/logic_proto/logic/scene_client_player.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/service/logic_proto/logic/scene_server_player.h"
#include "logic_proto/scene_normal.pb.h"
#include "src/service/logic_proto/logic/scene_normal.h"
#include "logic_proto/team_client_player.pb.h"
#include "src/service/logic_proto/logic/team_client_player.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/service/logic_proto/logic/team_server_player.h"
#include "logic_proto/scene_notplayer.pb.h"
#include "src/service/logic_proto/logic/scene_notplayer.h"
#include "logic_proto/team_notplayer.pb.h"
#include "src/service/logic_proto/logic/team_notplayer.h"
namespace game
{
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
std::unordered_set<std::string> g_open_player_services;
class PlayerSceneServiceImpl : public clientplayer::PlayerSceneService{};
class PlayerTeamServiceImpl : public clientplayer::PlayerTeamService{};
void InitPlayerServcie()
{
    g_player_services.emplace("msplayer.PlayerSceneService", std::make_unique<msplayer::PlayerPlayerSceneServiceImpl>(new PlayerSceneServiceImpl));
    g_player_services.emplace("msplayer.PlayerTeamService", std::make_unique<msplayer::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_player_services.emplace("clientplayer.PlayerSceneService", std::make_unique<clientplayer::PlayerPlayerSceneServiceImpl>(new PlayerSceneServiceImpl));
    g_player_services.emplace("serverplayer.PlayerSceneService", std::make_unique<serverplayer::PlayerPlayerSceneServiceImpl>(new PlayerSceneServiceImpl));
    g_player_services.emplace("serverplayer.PlayerTeamService", std::make_unique<serverplayer::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_player_services.emplace("clientplayer.PlayerTeamService", std::make_unique<clientplayer::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_player_services.emplace("gsplayer.PlayerTeamService", std::make_unique<gsplayer::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_player_services.emplace("gsplayer.PlayerTeamService", std::make_unique<gsplayer::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_player_services.emplace("gsplayer.PlayerTeamService", std::make_unique<gsplayer::PlayerPlayerTeamServiceImpl>(new PlayerTeamServiceImpl));
    g_open_player_services.emplace("clientplayer.PlayerSceneService");
PlayerSceneServiceImpl));
    g_open_player_services.emplace("clientplayer.PlayerTeamService");
PlayerTeamServiceImpl));
}
}//namespace game
