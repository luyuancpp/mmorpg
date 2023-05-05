#include "common_client_player_handler.h"
#include "scene_client_player_handler.h"
#include "team_client_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_player_services;
void InitPlayerService()
{
	g_player_services.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceHandler>());
	g_player_services.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>());
	g_player_services.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceHandler>());
}