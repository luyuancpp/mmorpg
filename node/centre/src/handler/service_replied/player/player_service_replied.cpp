#include <memory>
#include <unordered_map>
#include "service/player_service_replied.h"
#include "game_player_scene_replied_handler.h"
#include "game_player_replied_handler.h"
#include "player_state_attribute_sync_replied_handler.h"
class GamePlayerSceneServiceImpl : public GamePlayerSceneService {};
class GamePlayerServiceImpl : public GamePlayerService {};
class PlayerSyncServiceImpl : public PlayerSyncService {};

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> gPlayerServiceReplied;

void InitPlayerServiceReplied()
{
    gPlayerServiceReplied.emplace("GamePlayerSceneService", std::make_unique<GamePlayerSceneServiceRepliedHandler>(std::make_unique<GamePlayerSceneServiceImpl>()));
    gPlayerServiceReplied.emplace("GamePlayerService", std::make_unique<GamePlayerServiceRepliedHandler>(std::make_unique<GamePlayerServiceImpl>()));
    gPlayerServiceReplied.emplace("PlayerSyncService", std::make_unique<PlayerSyncServiceRepliedHandler>(std::make_unique<PlayerSyncServiceImpl>()));
}
