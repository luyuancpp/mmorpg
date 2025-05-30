
#include <unordered_map>
#include <memory>
#include <google/protobuf/service.h>
#include "centre_scene_handler.h"


#include "centre_service_handler.h"



extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void InitServiceHandler()
{
	gNodeService.emplace("CentreSceneService", std::make_unique_for_overwrite<CentreSceneServiceHandler>());

	gNodeService.emplace("CentreService", std::make_unique_for_overwrite<CentreServiceHandler>());

}
