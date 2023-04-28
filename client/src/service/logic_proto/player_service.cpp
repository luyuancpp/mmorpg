#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/common_client_player.pb.h"
#include "src/service/logic_proto/common_client_player.h"
#include "logic_proto/scene_client_player.pb.h"
#include "src/service/logic_proto/scene_client_player.h"
#include "logic_proto/team_client_player.pb.h"
#include "src/service/logic_proto/team_client_player.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
class ClientPlayerCommonServiceImpl : public ClientPlayerCommonService{};
class ClientPlayerSceneServiceHandler : public ClientPlayerSceneService{};
class ClientPlayerTeamServiceImpl : public ClientPlayerTeamService{};
void InitPlayerServcie()
{
    g_player_services.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceService>(new ClientPlayerCommonServiceImpl));
    g_player_services.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceService>(new ClientPlayerSceneServiceHandler));
    g_player_services.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceService>(new ClientPlayerTeamServiceImpl));
}
