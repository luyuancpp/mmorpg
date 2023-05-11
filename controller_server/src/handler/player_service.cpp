#include <memory>
#include <unordered_map>
#include "player_service.h"

#include "controller_common_server_player_handler.h"
#include "controller_scene_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_service;

class ConstrollerServerPlayerServiceImpl : public ConstrollerServerPlayerService{};
class ControllerSceneServerPlayerServiceImpl : public ControllerSceneServerPlayerService{};
void InitPlayerService()
{
	g_player_service.emplace("ConstrollerServerPlayerService", std::make_unique<ConstrollerServerPlayerServiceHandler>(new ConstrollerServerPlayerServiceImpl));
	g_player_service.emplace("ControllerSceneServerPlayerService", std::make_unique<ControllerSceneServerPlayerServiceHandler>(new ControllerSceneServerPlayerServiceImpl));
}