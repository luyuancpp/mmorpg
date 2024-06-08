#include "centre_scene_handler.h"
#include "controller_service_handler.h"

std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_server_service;

void InitServiceHandler()
{
	g_server_service.emplace("ControllerSceneService", std::make_unique_for_overwrite<CentreSceneServiceHandler>());
	g_server_service.emplace("ControllerService", std::make_unique_for_overwrite<CentreServiceHandler>());
}