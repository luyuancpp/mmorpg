#include <memory>
#include <string>
#include <unordered_map>
#include "rpc/player_service_interface.h"
#include "client_player_common_handler.h"
#include "player_currency_handler.h"
#include "player_lifecycle_handler.h"
#include "player_rollback_handler.h"
#include "player_scene_handler.h"
#include "player_skill_handler.h"
#include "player_state_attribute_sync_handler.h"
#include "s2s_player_scene_handler.h"
class SceneClientPlayerCommonImpl : public SceneClientPlayerCommon {};
class SceneCurrencyClientPlayerImpl : public SceneCurrencyClientPlayer {};
class ScenePlayerImpl : public ScenePlayer {};
class SceneRollbackClientPlayerImpl : public SceneRollbackClientPlayer {};
class SceneSceneClientPlayerImpl : public SceneSceneClientPlayer {};
class SceneSkillClientPlayerImpl : public SceneSkillClientPlayer {};
class ScenePlayerSyncImpl : public ScenePlayerSync {};
class SceneScenePlayerImpl : public SceneScenePlayer {};

std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;

void InitPlayerService()
{
    gPlayerService.emplace("SceneClientPlayerCommon", std::make_unique<SceneClientPlayerCommonHandler>(std::make_unique<SceneClientPlayerCommonImpl>()));
    gPlayerService.emplace("SceneCurrencyClientPlayer", std::make_unique<SceneCurrencyClientPlayerHandler>(std::make_unique<SceneCurrencyClientPlayerImpl>()));
    gPlayerService.emplace("ScenePlayer", std::make_unique<ScenePlayerHandler>(std::make_unique<ScenePlayerImpl>()));
    gPlayerService.emplace("SceneRollbackClientPlayer", std::make_unique<SceneRollbackClientPlayerHandler>(std::make_unique<SceneRollbackClientPlayerImpl>()));
    gPlayerService.emplace("SceneSceneClientPlayer", std::make_unique<SceneSceneClientPlayerHandler>(std::make_unique<SceneSceneClientPlayerImpl>()));
    gPlayerService.emplace("SceneSkillClientPlayer", std::make_unique<SceneSkillClientPlayerHandler>(std::make_unique<SceneSkillClientPlayerImpl>()));
    gPlayerService.emplace("ScenePlayerSync", std::make_unique<ScenePlayerSyncHandler>(std::make_unique<ScenePlayerSyncImpl>()));
    gPlayerService.emplace("SceneScenePlayer", std::make_unique<SceneScenePlayerHandler>(std::make_unique<SceneScenePlayerImpl>()));
}
