#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "centre_server_player_scene_handler.h"
#include "centre_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

class CentrePlayerSceneServiceImpl : public CentrePlayerSceneService{};
class CentrePlayerServiceImpl : public CentrePlayerService{};
void InitPlayerService()
{
	g_player_service.emplace("CentrePlayerSceneService", std::make_unique<CentrePlayerSceneServiceHandler>(new CentrePlayerSceneServiceImpl));
	g_player_service.emplace("CentrePlayerService", std::make_unique<CentrePlayerServiceHandler>(new CentrePlayerServiceImpl));
}