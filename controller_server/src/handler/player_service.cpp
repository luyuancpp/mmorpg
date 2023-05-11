#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "common_client_player_handler.h"
#include "scene_client_player_handler.h"
#include "team_client_player_handler.h"
#include "controller_common_server_player_handler.h"
#include "controller_scene_server_player_handler.h"
#include "game_scene_server_player_handler.h"
#include "game_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;

class ClientPlayerCommonServiceImpl : public ClientPlayerCommonService{};
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService{};
class ClientPlayerTeamServiceImpl : public ClientPlayerTeamService{};
class ConstrollerServerPlayerServiceImpl : public ConstrollerServerPlayerService{};
class ControllerSceneServerPlayerServiceImpl : public ControllerSceneServerPlayerService{};
class GameServerPlayerSceneServiceImpl : public GameServerPlayerSceneService{};
class ServerPlayerLoginServiceImpl : public ServerPlayerLoginService{};
void InitPlayerService()
{
	g_player_services.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceHandler>(new ClientPlayerCommonServiceImpl));
	g_player_services.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>(new ClientPlayerSceneServiceImpl));
	g_player_services.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceHandler>(new ClientPlayerTeamServiceImpl));
	g_player_services.emplace("ConstrollerServerPlayerService", std::make_unique<ConstrollerServerPlayerServiceHandler>(new ConstrollerServerPlayerServiceImpl));
	g_player_services.emplace("ControllerSceneServerPlayerService", std::make_unique<ControllerSceneServerPlayerServiceHandler>(new ControllerSceneServerPlayerServiceImpl));
	g_player_services.emplace("GameServerPlayerSceneService", std::make_unique<GameServerPlayerSceneServiceHandler>(new GameServerPlayerSceneServiceImpl));
	g_player_services.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceHandler>(new ServerPlayerLoginServiceImpl));
}