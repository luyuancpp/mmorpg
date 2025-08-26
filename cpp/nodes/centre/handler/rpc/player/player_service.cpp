#include <memory>
#include <string>
#include <unordered_map>
#include "rpc/player_service.h"
#include "centre_player_handler.h"
#include "centre_player_scene_handler.h"
class CentrePlayerUtilityImpl : public CentrePlayerUtility {};
class CentrePlayerSceneImpl : public CentrePlayerScene {};

std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;

void InitPlayerService()
{
    gPlayerService.emplace("CentrePlayerUtility", std::make_unique<CentrePlayerUtilityHandler>(std::make_unique<CentrePlayerUtilityImpl>()));
    gPlayerService.emplace("CentrePlayerScene", std::make_unique<CentrePlayerSceneHandler>(std::make_unique<CentrePlayerSceneImpl>()));
}
