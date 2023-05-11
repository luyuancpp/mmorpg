#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "controller_common_server_player_handler.h"
#include "controller_scene_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

class ConstrollerPlayerServiceImpl : public ConstrollerPlayerService{};
class ControllerScenePlayerServiceImpl : public ControllerScenePlayerService{};
void InitPlayerService()
{
	g_player_service.emplace("ConstrollerPlayerService", std::make_unique<ConstrollerPlayerServiceHandler>(new ConstrollerPlayerServiceImpl));
	g_player_service.emplace("ControllerScenePlayerService", std::make_unique<ControllerScenePlayerServiceHandler>(new ControllerScenePlayerServiceImpl));
}