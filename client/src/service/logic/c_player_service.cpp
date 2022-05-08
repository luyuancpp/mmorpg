#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/team_client_player.pb.h"
#include "src/service/logic/c_team_client_player.h"
#include "logic_proto/scene_client_player.pb.h"
#include "src/service/logic/c_scene_client_player.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
class ClientPlayerTeamServiceImpl : public ClientPlayerTeamService{};
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService{};
void InitPlayerServcie()
{
    g_player_services.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceService>(new ClientPlayerTeamServiceImpl));
    g_player_services.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceService>(new ClientPlayerSceneServiceImpl));
}
