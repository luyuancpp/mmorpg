#include <memory>
#include <string>
#include <unordered_map>
#include "service/player_service.h"
#include "game_player_scene_handler.h"
#include "game_player_handler.h"
#include "player_common_handler.h"
class GamePlayerSceneServiceImpl : public GamePlayerSceneService {};
class GamePlayerServiceImpl : public GamePlayerService {};
class PlayerClientCommonServiceImpl : public PlayerClientCommonService {};

std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

void InitPlayerService()
{
    g_player_service.emplace("GamePlayerSceneService", std::make_unique<GamePlayerSceneServiceHandler>(std::make_unique<GamePlayerSceneServiceImpl>()));
    g_player_service.emplace("GamePlayerService", std::make_unique<GamePlayerServiceHandler>(std::make_unique<GamePlayerServiceImpl>()));
    g_player_service.emplace("PlayerClientCommonService", std::make_unique<PlayerClientCommonServiceHandler>(std::make_unique<PlayerClientCommonServiceImpl>()));
}
