#include "centre_scene_handler.h"
#include "centre_service_handler.h"

std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void InitServiceHandler()
{
%!g(string=	)NodeService.emplace("CentreSceneService", std::make_unique_for_overwrite<CentreSceneServiceHandler>());
%!g(string=	)NodeService.emplace("CentreService", std::make_unique_for_overwrite<CentreServiceHandler>());
}