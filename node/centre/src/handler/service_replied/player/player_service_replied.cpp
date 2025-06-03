#include <memory>
#include <unordered_map>
#include "service/player_service_replied.h"
#include "game_scene_replied_handler.h"
#include "game_service_replied_handler.h"
class GameSceneServiceImpl : public GameSceneService {};
class GameServiceImpl : public GameService {};

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;

void InitPlayerServiceReplied()
{
    g_player_service_replied.emplace("GameSceneService", std::make_unique<GameSceneServiceRepliedHandler>(std::make_unique<GameSceneServiceImpl>()));
    g_player_service_replied.emplace("GameService", std::make_unique<GameServiceRepliedHandler>(std::make_unique<GameServiceImpl>()));
}
