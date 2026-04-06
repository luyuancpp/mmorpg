#include <memory>
#include <unordered_map>
#include "rpc/player_rpc_response_handler.h"
#include "player_currency_response_handler.h"
#include "player_lifecycle_response_handler.h"
#include "player_rollback_response_handler.h"
#include "player_state_attribute_sync_response_handler.h"
#include "s2s_player_scene_response_handler.h"
class SceneCurrencyClientPlayerImpl : public SceneCurrencyClientPlayer {};
class ScenePlayerImpl : public ScenePlayer {};
class SceneRollbackClientPlayerImpl : public SceneRollbackClientPlayer {};
class ScenePlayerSyncImpl : public ScenePlayerSync {};
class SceneScenePlayerImpl : public SceneScenePlayer {};

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> gPlayerServiceReplied;

void InitPlayerServiceReplied()
{
    gPlayerServiceReplied.emplace("SceneCurrencyClientPlayer", std::make_unique<SceneCurrencyClientPlayerReply>(std::make_unique<SceneCurrencyClientPlayerImpl>()));
    gPlayerServiceReplied.emplace("ScenePlayer", std::make_unique<ScenePlayerReply>(std::make_unique<ScenePlayerImpl>()));
    gPlayerServiceReplied.emplace("SceneRollbackClientPlayer", std::make_unique<SceneRollbackClientPlayerReply>(std::make_unique<SceneRollbackClientPlayerImpl>()));
    gPlayerServiceReplied.emplace("ScenePlayerSync", std::make_unique<ScenePlayerSyncReply>(std::make_unique<ScenePlayerSyncImpl>()));
    gPlayerServiceReplied.emplace("SceneScenePlayer", std::make_unique<SceneScenePlayerReply>(std::make_unique<SceneScenePlayerImpl>()));
}
