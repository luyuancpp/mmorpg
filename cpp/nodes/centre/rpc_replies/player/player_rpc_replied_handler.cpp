#include <memory>
#include <unordered_map>
#include "rpc/player_rpc_response_handler.h"
#include "game_player_response_handler.h"
#include "game_player_scene_response_handler.h"
#include "player_state_attribute_sync_response_handler.h"
class ScenePlayerImpl : public ScenePlayer {};
class SceneScenePlayerImpl : public SceneScenePlayer {};
class ScenePlayerSyncImpl : public ScenePlayerSync {};

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> gPlayerServiceReplied;

void InitPlayerServiceReplied()
{
    gPlayerServiceReplied.emplace("ScenePlayer", std::make_unique<ScenePlayerReply>(std::make_unique<ScenePlayerImpl>()));
    gPlayerServiceReplied.emplace("SceneScenePlayer", std::make_unique<SceneScenePlayerReply>(std::make_unique<SceneScenePlayerImpl>()));
    gPlayerServiceReplied.emplace("ScenePlayerSync", std::make_unique<ScenePlayerSyncReply>(std::make_unique<ScenePlayerSyncImpl>()));
}
