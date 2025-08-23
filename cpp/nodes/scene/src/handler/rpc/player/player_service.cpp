#include <memory>
#include <string>
#include <unordered_map>
#include "rpc/player_service.h"
#include "game_client_player_handler.h"
#include "game_player_handler.h"
#include "game_player_scene_handler.h"
#include "player_scene_handler.h"
#include "player_skill_handler.h"
#include "player_state_attribute_sync_handler.h"
class SceneClientPlayerCommonImpl : public SceneClientPlayerCommon {};
class ScenePlayerImpl : public ScenePlayer {};
class SceneScenePlayerImpl : public SceneScenePlayer {};
class SceneSceneClientPlayerImpl : public SceneSceneClientPlayer {};
class SceneSkillClientPlayerImpl : public SceneSkillClientPlayer {};
class ScenePlayerSyncImpl : public ScenePlayerSync {};

std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;

void InitPlayerService()
{
    gPlayerService.emplace("SceneClientPlayerCommon", std::make_unique<SceneClientPlayerCommonHandler>(std::make_unique<SceneClientPlayerCommonImpl>()));
    gPlayerService.emplace("ScenePlayer", std::make_unique<ScenePlayerHandler>(std::make_unique<ScenePlayerImpl>()));
    gPlayerService.emplace("SceneScenePlayer", std::make_unique<SceneScenePlayerHandler>(std::make_unique<SceneScenePlayerImpl>()));
    gPlayerService.emplace("SceneSceneClientPlayer", std::make_unique<SceneSceneClientPlayerHandler>(std::make_unique<SceneSceneClientPlayerImpl>()));
    gPlayerService.emplace("SceneSkillClientPlayer", std::make_unique<SceneSkillClientPlayerHandler>(std::make_unique<SceneSkillClientPlayerImpl>()));
    gPlayerService.emplace("ScenePlayerSync", std::make_unique<ScenePlayerSyncHandler>(std::make_unique<ScenePlayerSyncImpl>()));
}
