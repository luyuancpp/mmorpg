#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/common_client_player.pb.h"
#include "src/handler/common_client_player_handler.h"
#include "logic_proto/common_server_player.pb.h"
#include "src/handler/common_server_player_handler.h"
#include "logic_proto/scene_client_player.pb.h"
#include "src/handler/scene_client_player_handler.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/handler/scene_server_player_handler.h"
#include "logic_proto/team_client_player.pb.h"
#include "src/handler/team_client_player_handler.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/handler/team_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
class ClientPlayerCommonServiceOpenImpl : public ClientPlayerCommonService{};
class ServerPlayerLoginServiceOpenImpl : public ServerPlayerLoginService{};
class ClientPlayerSceneServiceOpenImpl : public ClientPlayerSceneService{};
class ServerPlayerSceneServiceOpenImpl : public ServerPlayerSceneService{};
class ClientPlayerTeamServiceOpenImpl : public ClientPlayerTeamService{};
class ServerPlayerTeamServiceOpenImpl : public ServerPlayerTeamService{};
void InitPlayerService()
{
    g_player_services.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceHandler>(new ClientPlayerCommonServiceOpenImpl));
    g_player_services.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceHandler>(new ServerPlayerLoginServiceOpenImpl));
    g_player_services.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>(new ClientPlayerSceneServiceOpenImpl));
    g_player_services.emplace("ServerPlayerSceneService", std::make_unique<ServerPlayerSceneServiceHandler>(new ServerPlayerSceneServiceOpenImpl));
    g_player_services.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceHandler>(new ClientPlayerTeamServiceOpenImpl));
    g_player_services.emplace("ServerPlayerTeamService", std::make_unique<ServerPlayerTeamServiceHandler>(new ServerPlayerTeamServiceOpenImpl));
}
