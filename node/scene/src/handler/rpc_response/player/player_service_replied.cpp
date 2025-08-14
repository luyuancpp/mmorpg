#include <memory>
#include <unordered_map>
#include "service/player_service_replied.h"
#include "centre_player_replied_handler.h"
#include "centre_player_scene_replied_handler.h"
class CentrePlayerUtilityImpl : public CentrePlayerUtility {};
class CentrePlayerSceneImpl : public CentrePlayerScene {};

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> gPlayerServiceReplied;

void InitPlayerServiceReplied()
{
    gPlayerServiceReplied.emplace("CentrePlayerUtility", std::make_unique<CentrePlayerUtilityRepliedHandler>(std::make_unique<CentrePlayerUtilityImpl>()));
    gPlayerServiceReplied.emplace("CentrePlayerScene", std::make_unique<CentrePlayerSceneRepliedHandler>(std::make_unique<CentrePlayerSceneImpl>()));
}
