#include <memory>
#include <string>
#include <unordered_map>
#include "service/player_service.h"
#include "player_scene_handler.h"
#include "player_skill_handler.h"
#include "game_client_player_handler.h"
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService {};
class ClientPlayerSkillServiceImpl : public ClientPlayerSkillService {};
class GameClientPlayerCommonServiceImpl : public GameClientPlayerCommonService {};

std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;

void InitPlayerService()
{
    gPlayerService.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>(std::make_unique<ClientPlayerSceneServiceImpl>()));
    gPlayerService.emplace("ClientPlayerSkillService", std::make_unique<ClientPlayerSkillServiceHandler>(std::make_unique<ClientPlayerSkillServiceImpl>()));
    gPlayerService.emplace("GameClientPlayerCommonService", std::make_unique<GameClientPlayerCommonServiceHandler>(std::make_unique<GameClientPlayerCommonServiceImpl>()));
}
