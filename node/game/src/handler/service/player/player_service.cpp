#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "player_scene_handler.h"
#include "game_player_scene_handler.h"
#include "game_player_handler.h"
#include "player_common_handler.h"
#include "player_skill_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService{};
class GamePlayerSceneServiceImpl : public GamePlayerSceneService{};
class GamePlayerServiceImpl : public GamePlayerService{};
class PlayerClientCommonServiceImpl : public PlayerClientCommonService{};
class PlayerSkillServiceImpl : public PlayerSkillService{};
void InitPlayerService()
{
	g_player_service.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>(new ClientPlayerSceneServiceImpl));
	g_player_service.emplace("GamePlayerSceneService", std::make_unique<GamePlayerSceneServiceHandler>(new GamePlayerSceneServiceImpl));
	g_player_service.emplace("GamePlayerService", std::make_unique<GamePlayerServiceHandler>(new GamePlayerServiceImpl));
	g_player_service.emplace("PlayerClientCommonService", std::make_unique<PlayerClientCommonServiceHandler>(new PlayerClientCommonServiceImpl));
	g_player_service.emplace("PlayerSkillService", std::make_unique<PlayerSkillServiceHandler>(new PlayerSkillServiceImpl));
}