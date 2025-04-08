#include "game_scene_handler.h"
#include "game_service_handler.h"

std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void InitServiceHandler()
{
	gNodeService.emplace("GameSceneService", std::make_unique_for_overwrite<GameSceneServiceHandler>());
	gNodeService.emplace("GameService", std::make_unique_for_overwrite<GameServiceHandler>());
}