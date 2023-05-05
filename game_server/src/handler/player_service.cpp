#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "common_client_player_handler.h"
#include "scene_client_player_handler.h"
#include "team_client_player_handler.h"
#include "common_server_player_handler.h"
#include "scene_server_player_handler.h"
#include "team_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;

class ClientPlayerCommonServiceImpl : public ClientPlayerCommonService{};
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService{};
class ClientPlayerTeamServiceImpl : public ClientPlayerTeamService{};
class ServerPlayerLoginServiceImpl : public ServerPlayerLoginService{};
class ServerPlayerSceneServiceImpl : public ServerPlayerSceneService{};
class ServerPlayerTeamServiceImpl : public ServerPlayerTeamService{};
void InitPlayerService()
{
	g_player_services.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceHandler>(new ClientPlayerCommonServiceImpl));
	g_player_services.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>(new ClientPlayerSceneServiceImpl));
	g_player_services.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceHandler>(new ClientPlayerTeamServiceImpl));
	g_player_services.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceHandler>(new ServerPlayerLoginServiceImpl));
	g_player_services.emplace("ServerPlayerSceneService", std::make_unique<ServerPlayerSceneServiceHandler>(new ServerPlayerSceneServiceImpl));
	g_player_services.emplace("ServerPlayerTeamService", std::make_unique<ServerPlayerTeamServiceHandler>(new ServerPlayerTeamServiceImpl));
}