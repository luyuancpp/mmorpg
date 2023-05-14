#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "game_scene_server_player_handler.h"
#include "game_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

class GamePlayerSceneServiceImpl : public GamePlayerSceneService{};
class GamePlayerServiceImpl : public GamePlayerService{};
void InitPlayerService()
{
	g_player_service.emplace("GamePlayerSceneService", std::make_unique<GamePlayerSceneServiceHandler>(new GamePlayerSceneServiceImpl));
	g_player_service.emplace("GamePlayerService", std::make_unique<GamePlayerServiceHandler>(new GamePlayerServiceImpl));
}