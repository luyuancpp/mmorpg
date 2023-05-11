#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "game_scene_server_player_handler.h"
#include "game_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

class GameServerPlayerSceneServiceImpl : public GameServerPlayerSceneService{};
class ServerPlayerLoginServiceImpl : public ServerPlayerLoginService{};
void InitPlayerService()
{
	g_player_service.emplace("GameServerPlayerSceneService", std::make_unique<GameServerPlayerSceneServiceHandler>(new GameServerPlayerSceneServiceImpl));
	g_player_service.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceHandler>(new ServerPlayerLoginServiceImpl));
}