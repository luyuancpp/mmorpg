#include <memory>
#include <unordered_map>
#include "player_service_replied.h"

#include "common_client_player_replied_handler.h"
#include "scene_client_player_replied_handler.h"
#include "team_client_player_replied_handler.h"
#include "game_scene_server_player_replied_handler.h"
#include "game_server_player_replied_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;

class ClientPlayerCommonServiceImpl : public ClientPlayerCommonService{};
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService{};
class ClientPlayerTeamServiceImpl : public ClientPlayerTeamService{};
class GameServerPlayerSceneServiceImpl : public GameServerPlayerSceneService{};
class ServerPlayerLoginServiceImpl : public ServerPlayerLoginService{};
void InitPlayerServiceReplied()
{
	g_player_service_replied.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceRepliedHandler>(new ClientPlayerCommonServiceImpl));
	g_player_service_replied.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceRepliedHandler>(new ClientPlayerSceneServiceImpl));
	g_player_service_replied.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceRepliedHandler>(new ClientPlayerTeamServiceImpl));
	g_player_service_replied.emplace("GameServerPlayerSceneService", std::make_unique<GameServerPlayerSceneServiceRepliedHandler>(new GameServerPlayerSceneServiceImpl));
	g_player_service_replied.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceRepliedHandler>(new ServerPlayerLoginServiceImpl));
}