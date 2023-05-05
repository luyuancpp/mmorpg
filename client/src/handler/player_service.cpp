#include "client_player_proto/common_client_player.pb.h"
#include "client_player_proto/scene_client_player.pb.h"
#include "client_player_proto/team_client_player.pb.h"
std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> g_player_services;
void InitPlayerService()
{
	g_player_services.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceHandler>());
	g_player_services.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>());
	g_player_services.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceHandler>());
}