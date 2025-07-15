#include <memory>
#include <unordered_map>
#include "service/player_service_replied.h"
#include "game_client_player_replied_handler.h"
#include "game_player_replied_handler.h"
#include "game_player_scene_replied_handler.h"
#include "player_scene_replied_handler.h"
#include "player_skill_replied_handler.h"
#include "player_state_attribute_sync_replied_handler.h"
class SceneClientPlayerCommonImpl : public SceneClientPlayerCommon {};
class ScenePlayerImpl : public ScenePlayer {};
class SceneScenePlayerImpl : public SceneScenePlayer {};
class SceneSceneClientPlayerImpl : public SceneSceneClientPlayer {};
class SceneSkillClientPlayerImpl : public SceneSkillClientPlayer {};
class ScenePlayerSyncImpl : public ScenePlayerSync {};

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> gPlayerServiceReplied;

void InitPlayerServiceReplied()
{
    gPlayerServiceReplied.emplace("SceneClientPlayerCommon", std::make_unique<SceneClientPlayerCommonRepliedHandler>(std::make_unique<SceneClientPlayerCommonImpl>()));
    gPlayerServiceReplied.emplace("ScenePlayer", std::make_unique<ScenePlayerRepliedHandler>(std::make_unique<ScenePlayerImpl>()));
    gPlayerServiceReplied.emplace("SceneScenePlayer", std::make_unique<SceneScenePlayerRepliedHandler>(std::make_unique<SceneScenePlayerImpl>()));
    gPlayerServiceReplied.emplace("SceneSceneClientPlayer", std::make_unique<SceneSceneClientPlayerRepliedHandler>(std::make_unique<SceneSceneClientPlayerImpl>()));
    gPlayerServiceReplied.emplace("SceneSkillClientPlayer", std::make_unique<SceneSkillClientPlayerRepliedHandler>(std::make_unique<SceneSkillClientPlayerImpl>()));
    gPlayerServiceReplied.emplace("ScenePlayerSync", std::make_unique<ScenePlayerSyncRepliedHandler>(std::make_unique<ScenePlayerSyncImpl>()));
}
