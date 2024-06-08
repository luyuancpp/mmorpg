#include <memory>
#include <unordered_map>
#include "player_service_replied.h"

#include "centre_common_server_player_replied_handler.h"
#include "centre_scene_server_player_replied_handler.h"
#include "common_client_player_replied_handler.h"
#include "scene_client_player_replied_handler.h"
#include "team_client_player_replied_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;

class CentrePlayerServiceImpl : public CentrePlayerService{};
class CentreScenePlayerServiceImpl : public CentreScenePlayerService{};
class ClientPlayerCommonServiceImpl : public ClientPlayerCommonService{};
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService{};
class ClientPlayerTeamServiceImpl : public ClientPlayerTeamService{};
void InitPlayerServiceReplied()
{
	g_player_service_replied.emplace("CentrePlayerService", std::make_unique<CentrePlayerServiceRepliedHandler>(new CentrePlayerServiceImpl));
	g_player_service_replied.emplace("CentreScenePlayerService", std::make_unique<CentreScenePlayerServiceRepliedHandler>(new CentreScenePlayerServiceImpl));
	g_player_service_replied.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceRepliedHandler>(new ClientPlayerCommonServiceImpl));
	g_player_service_replied.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceRepliedHandler>(new ClientPlayerSceneServiceImpl));
	g_player_service_replied.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceRepliedHandler>(new ClientPlayerTeamServiceImpl));
}