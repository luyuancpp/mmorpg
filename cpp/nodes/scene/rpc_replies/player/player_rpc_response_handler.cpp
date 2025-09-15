#include <memory>
#include <unordered_map>
#include "rpc/player_rpc_response_handler.h"
#include "player_locator_response_handler.h"
#include "centre_player_response_handler.h"
#include "centre_player_scene_response_handler.h"
class PlayerLocatorImpl : public PlayerLocator {};
class CentrePlayerUtilityImpl : public CentrePlayerUtility {};
class CentrePlayerSceneImpl : public CentrePlayerScene {};

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> gPlayerServiceReplied;

void InitPlayerServiceReplied()
{
    gPlayerServiceReplied.emplace("PlayerLocator", std::make_unique<PlayerLocatorReply>(std::make_unique<PlayerLocatorImpl>()));
    gPlayerServiceReplied.emplace("CentrePlayerUtility", std::make_unique<CentrePlayerUtilityReply>(std::make_unique<CentrePlayerUtilityImpl>()));
    gPlayerServiceReplied.emplace("CentrePlayerScene", std::make_unique<CentrePlayerSceneReply>(std::make_unique<CentrePlayerSceneImpl>()));
}
