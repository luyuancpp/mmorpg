#include <memory>
#include <unordered_map>
#include "service/player_service_replied.h"
#include "centre_player_scene_replied_handler.h"
#include "centre_player_replied_handler.h"
class CentrePlayerSceneServiceImpl : public CentrePlayerSceneService {};
class CentrePlayerServiceImpl : public CentrePlayerService {};

std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;

void InitPlayerServiceReplied()
{
    g_player_service_replied.emplace("CentrePlayerSceneService", std::make_unique<CentrePlayerSceneServiceRepliedHandler>(std::make_unique<CentrePlayerSceneServiceImpl>()));
    g_player_service_replied.emplace("CentrePlayerService", std::make_unique<CentrePlayerServiceRepliedHandler>(std::make_unique<CentrePlayerServiceImpl>()));
}
