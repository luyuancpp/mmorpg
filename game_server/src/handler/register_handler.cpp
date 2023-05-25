#include "game_scene_handler.h"
#include "game_service_handler.h"

std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_server_service;

void InitServiceHandler()
{
	g_server_service.emplace("GameSceneService", std::make_unique_for_overwrite<GameSceneServiceHandler>());
	g_server_service.emplace("GameService", std::make_unique_for_overwrite<GameServiceHandler>());
}