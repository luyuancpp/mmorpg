#include "common_client_player_handler.h"
#include "scene_client_player_handler.h"
#include "team_client_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_player_service;
void InitPlayerService()
{
	g_player_service.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceHandler>());
	g_player_service.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>());
	g_player_service.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceHandler>());
}