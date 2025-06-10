
#include <unordered_map>
#include <memory>
#include <google/protobuf/service.h>
#include "centre_scene_handler.h"


#include "centre_service_handler.h"



extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void InitServiceHandler()
{
	gNodeService.emplace("CentreScene", std::make_unique_for_overwrite<CentreSceneHandler>());

	gNodeService.emplace("Centre", std::make_unique_for_overwrite<CentreHandler>());

}
