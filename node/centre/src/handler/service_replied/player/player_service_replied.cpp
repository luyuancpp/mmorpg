#include <memory>
#include <unordered_map>
#include "player_service_replied.h"

#include "game_player_scene_replied_handler.h"
#include "game_player_replied_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;

class GamePlayerSceneServiceImpl : public GamePlayerSceneService{};
class GamePlayerServiceImpl : public GamePlayerService{};
void InitPlayerServiceReplied()
{
	auto aa = std::make_unique<GamePlayerSceneServiceImpl>();
	g_player_service_replied.emplace("GamePlayerSceneService", std::make_unique<GamePlayerSceneServiceRepliedHandler>(std::move(aa)));
	g_player_service_replied.emplace("GamePlayerService", std::make_unique<GamePlayerServiceRepliedHandler>(std::make_unique<GamePlayerServiceImpl>()));
}