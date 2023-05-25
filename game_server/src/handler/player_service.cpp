#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "common_client_player_handler.h"
#include "scene_client_player_handler.h"
#include "team_client_player_handler.h"
#include "game_scene_server_player_handler.h"
#include "game_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

class ClientPlayerCommonServiceImpl : public ClientPlayerCommonService{};
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService{};
class ClientPlayerTeamServiceImpl : public ClientPlayerTeamService{};
class GamePlayerSceneServiceImpl : public GamePlayerSceneService{};
class GamePlayerServiceImpl : public GamePlayerService{};
void InitPlayerService()
{
	g_player_service.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceHandler>(new ClientPlayerCommonServiceImpl));
	g_player_service.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>(new ClientPlayerSceneServiceImpl));
	g_player_service.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceHandler>(new ClientPlayerTeamServiceImpl));
	g_player_service.emplace("GamePlayerSceneService", std::make_unique<GamePlayerSceneServiceHandler>(new GamePlayerSceneServiceImpl));
	g_player_service.emplace("GamePlayerService", std::make_unique<GamePlayerServiceHandler>(new GamePlayerServiceImpl));
}