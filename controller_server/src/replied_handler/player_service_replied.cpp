#include <memory>
#include <unordered_map>
#include "player_service_replied.h"

#include "common_client_player_handler.h"
#include "scene_client_player_handler.h"
#include "team_client_player_handler.h"
#include "common_server_player_handler.h"
#include "scene_server_player_handler.h"
#include "team_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replieds;

class ClientPlayerCommonServiceImpl : public ClientPlayerCommonService{};
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService{};
class ClientPlayerTeamServiceImpl : public ClientPlayerTeamService{};
class ServerPlayerLoginServiceImpl : public ServerPlayerLoginService{};
class ServerPlayerSceneServiceImpl : public ServerPlayerSceneService{};
class ServerPlayerTeamServiceImpl : public ServerPlayerTeamService{};
void InitPlayerServiceReplied()
{
	g_player_service_replieds.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceRepliedHandler>(new ClientPlayerCommonServiceImpl));
	g_player_service_replieds.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceRepliedHandler>(new ClientPlayerSceneServiceImpl));
	g_player_service_replieds.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceRepliedHandler>(new ClientPlayerTeamServiceImpl));
	g_player_service_replieds.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceRepliedHandler>(new ServerPlayerLoginServiceImpl));
	g_player_service_replieds.emplace("ServerPlayerSceneService", std::make_unique<ServerPlayerSceneServiceRepliedHandler>(new ServerPlayerSceneServiceImpl));
	g_player_service_replieds.emplace("ServerPlayerTeamService", std::make_unique<ServerPlayerTeamServiceRepliedHandler>(new ServerPlayerTeamServiceImpl));
}