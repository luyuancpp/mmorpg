
#include <unordered_map>
#include <memory>
#include <google/protobuf/service.h>
#include "game_scene_handler.h"



extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void InitServiceHandler()
{
	gNodeService.emplace("GameSceneService", std::make_unique_for_overwrite<GameSceneServiceHandler>());

}
