#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "controller_common_server_player_handler.h"
#include "controller_scene_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

class ControllerPlayerServiceImpl : public ControllerPlayerService{};
class ControllerScenePlayerServiceImpl : public ControllerScenePlayerService{};
void InitPlayerService()
{
	g_player_service.emplace("ControllerPlayerService", std::make_unique<ControllerPlayerServiceHandler>(new ControllerPlayerServiceImpl));
	g_player_service.emplace("ControllerScenePlayerService", std::make_unique<ControllerScenePlayerServiceHandler>(new ControllerScenePlayerServiceImpl));
}