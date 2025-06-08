#include <memory>
#include <string>
#include <unordered_map>
#include "service/player_service.h"
#include "player_scene_handler.h"
#include "player_skill_handler.h"
#include "game_client_player_handler.h"
#include "game_player_scene_handler.h"
#include "game_player_handler.h"
#include "player_state_attribute_sync_handler.h"
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService {};
class ClientPlayerSkillServiceImpl : public ClientPlayerSkillService {};
class GameClientPlayerCommonServiceImpl : public GameClientPlayerCommonService {};
class GamePlayerSceneServiceImpl : public GamePlayerSceneService {};
class GamePlayerServiceImpl : public GamePlayerService {};
class PlayerSyncServiceImpl : public PlayerSyncService {};

std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;

void InitPlayerService()
{
    gPlayerService.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>(std::make_unique<ClientPlayerSceneServiceImpl>()));
    gPlayerService.emplace("ClientPlayerSkillService", std::make_unique<ClientPlayerSkillServiceHandler>(std::make_unique<ClientPlayerSkillServiceImpl>()));
    gPlayerService.emplace("GameClientPlayerCommonService", std::make_unique<GameClientPlayerCommonServiceHandler>(std::make_unique<GameClientPlayerCommonServiceImpl>()));
    gPlayerService.emplace("GamePlayerSceneService", std::make_unique<GamePlayerSceneServiceHandler>(std::make_unique<GamePlayerSceneServiceImpl>()));
    gPlayerService.emplace("GamePlayerService", std::make_unique<GamePlayerServiceHandler>(std::make_unique<GamePlayerServiceImpl>()));
    gPlayerService.emplace("PlayerSyncService", std::make_unique<PlayerSyncServiceHandler>(std::make_unique<PlayerSyncServiceImpl>()));
}
