#include <memory>
#include <unordered_map>
#include "player_service_replied.h"

#include "common_client_player_replied_handler.h"
#include "scene_client_player_replied_handler.h"
#include "team_client_player_replied_handler.h"
#include "controller_common_server_player_replied_handler.h"
#include "controller_scene_server_player_replied_handler.h"
#include "game_scene_server_player_replied_handler.h"
#include "game_server_player_replied_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replieds;

class ClientPlayerCommonServiceImpl : public ClientPlayerCommonService{};
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService{};
class ClientPlayerTeamServiceImpl : public ClientPlayerTeamService{};
class ConstrollerServerPlayerServiceImpl : public ConstrollerServerPlayerService{};
class ControllerSceneServerPlayerServiceImpl : public ControllerSceneServerPlayerService{};
class GameServerPlayerSceneServiceImpl : public GameServerPlayerSceneService{};
class ServerPlayerLoginServiceImpl : public ServerPlayerLoginService{};
void InitPlayerServiceReplied()
{
	g_player_service_replieds.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceRepliedHandler>(new ClientPlayerCommonServiceImpl));
	g_player_service_replieds.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceRepliedHandler>(new ClientPlayerSceneServiceImpl));
	g_player_service_replieds.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceRepliedHandler>(new ClientPlayerTeamServiceImpl));
	g_player_service_replieds.emplace("ConstrollerServerPlayerService", std::make_unique<ConstrollerServerPlayerServiceRepliedHandler>(new ConstrollerServerPlayerServiceImpl));
	g_player_service_replieds.emplace("ControllerSceneServerPlayerService", std::make_unique<ControllerSceneServerPlayerServiceRepliedHandler>(new ControllerSceneServerPlayerServiceImpl));
	g_player_service_replieds.emplace("GameServerPlayerSceneService", std::make_unique<GameServerPlayerSceneServiceRepliedHandler>(new GameServerPlayerSceneServiceImpl));
	g_player_service_replieds.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceRepliedHandler>(new ServerPlayerLoginServiceImpl));
}