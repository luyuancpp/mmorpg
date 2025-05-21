#include <memory>
#include <string>
#include <unordered_map>
#include "service/player_service.h"
#include "player_scene_handler.h"
#include "player_state_attribute_sync_handler.h"
#include "game_player_scene_handler.h"
#include "game_player_handler.h"
#include "player_common_handler.h"
#include "player_skill_handler.h"
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService {};
class EntitySyncServiceImpl : public EntitySyncService {};
class GamePlayerSceneServiceImpl : public GamePlayerSceneService {};
class GamePlayerServiceImpl : public GamePlayerService {};
class PlayerClientCommonServiceImpl : public PlayerClientCommonService {};
class PlayerSkillServiceImpl : public PlayerSkillService {};

std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

void InitPlayerService()
{
    g_player_service.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>(std::make_unique<ClientPlayerSceneServiceImpl>()));
    g_player_service.emplace("EntitySyncService", std::make_unique<EntitySyncServiceHandler>(std::make_unique<EntitySyncServiceImpl>()));
    g_player_service.emplace("GamePlayerSceneService", std::make_unique<GamePlayerSceneServiceHandler>(std::make_unique<GamePlayerSceneServiceImpl>()));
    g_player_service.emplace("GamePlayerService", std::make_unique<GamePlayerServiceHandler>(std::make_unique<GamePlayerServiceImpl>()));
    g_player_service.emplace("PlayerClientCommonService", std::make_unique<PlayerClientCommonServiceHandler>(std::make_unique<PlayerClientCommonServiceImpl>()));
    g_player_service.emplace("PlayerSkillService", std::make_unique<PlayerSkillServiceHandler>(std::make_unique<PlayerSkillServiceImpl>()));
}
