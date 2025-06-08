#include <memory>
#include <string>
#include <unordered_map>
#include "service/player_service.h"
#include "centre_player_scene_handler.h"
#include "centre_player_handler.h"
class CentrePlayerSceneImpl : public CentrePlayerScene {};
class CentrePlayerUtilityImpl : public CentrePlayerUtility {};

std::unordered_map<std::string, std::unique_ptr<PlayerService>> gPlayerService;

void InitPlayerService()
{
    gPlayerService.emplace("CentrePlayerScene", std::make_unique<CentrePlayerSceneHandler>(std::make_unique<CentrePlayerSceneImpl>()));
    gPlayerService.emplace("CentrePlayerUtility", std::make_unique<CentrePlayerUtilityHandler>(std::make_unique<CentrePlayerUtilityImpl>()));
}
