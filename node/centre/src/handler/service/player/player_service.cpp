#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "centre_server_player_scene_handler.h"
#include "centre_scene_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

class CentrePlayerServiceImpl : public CentrePlayerService{};
class CentreScenePlayerServiceImpl : public CentreScenePlayerService{};
void InitPlayerService()
{
	g_player_service.emplace("CentrePlayerService", std::make_unique<CentrePlayerServiceHandler>(new CentrePlayerServiceImpl));
	g_player_service.emplace("CentreScenePlayerService", std::make_unique<CentreScenePlayerServiceHandler>(new CentreScenePlayerServiceImpl));
}