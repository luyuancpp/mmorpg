#include <memory>
#include <unordered_map>
#include "service/player_service_replied.h"
#include "game_player_replied_handler.h"
#include "game_player_scene_replied_handler.h"
#include "player_state_attribute_sync_replied_handler.h"
class ScenePlayerImpl : public ScenePlayer {};
class SceneScenePlayerImpl : public SceneScenePlayer {};
class ScenePlayerSyncImpl : public ScenePlayerSync {};

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> gPlayerServiceReplied;

void InitPlayerServiceReplied()
{
    gPlayerServiceReplied.emplace("ScenePlayer", std::make_unique<ScenePlayerRepliedHandler>(std::make_unique<ScenePlayerImpl>()));
    gPlayerServiceReplied.emplace("SceneScenePlayer", std::make_unique<SceneScenePlayerRepliedHandler>(std::make_unique<SceneScenePlayerImpl>()));
    gPlayerServiceReplied.emplace("ScenePlayerSync", std::make_unique<ScenePlayerSyncRepliedHandler>(std::make_unique<ScenePlayerSyncImpl>()));
}
