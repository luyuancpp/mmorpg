
#include <unordered_map>
#include <memory>
#include <google/protobuf/service.h>
#include "game_service_handler.h"


#include "game_scene_handler.h"



extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void InitServiceHandler()
{
	gNodeService.emplace("Scene", std::make_unique_for_overwrite<SceneHandler>());

	gNodeService.emplace("SceneScene", std::make_unique_for_overwrite<SceneSceneHandler>());

}
