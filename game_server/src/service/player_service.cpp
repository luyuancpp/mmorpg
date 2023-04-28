#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/common_client_player.pb.h"
#include "src/service/common_client_player_handler.h"
#include "logic_proto/common_server_player.pb.h"
#include "src/service/common_server_player_handler.h"
#include "logic_proto/scene_client_player.pb.h"
#include "src/service/scene_client_player_handler.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/service/scene_server_player.h"
#include "logic_proto/team_client_player.pb.h"
#include "src/service/team_client_player.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/service/team_server_player.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
class ClientPlayerCommonServiceOpenImpl : public ClientPlayerCommonService{};
class ServerPlayerLoginServiceOpenImpl : public ServerPlayerLoginService{};
class ClientPlayerSceneServiceOpenImpl : public ClientPlayerSceneService{};
class ServerPlayerSceneServiceOpenImpl : public ServerPlayerSceneService{};
class ClientPlayerTeamServiceOpenImpl : public ClientPlayerTeamService{};
class ServerPlayerTeamServiceOpenImpl : public ServerPlayerTeamService{};
void InitPlayerServcie()
{
    g_player_services.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceHandler>(new ClientPlayerCommonServiceOpenImpl));
    g_player_services.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceHandler>(new ServerPlayerLoginServiceOpenImpl));
    g_player_services.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceHandler>(new ClientPlayerSceneServiceOpenImpl));
    g_player_services.emplace("ServerPlayerSceneService", std::make_unique<ServerPlayerSceneServiceImpl>(new ServerPlayerSceneServiceOpenImpl));
    g_player_services.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceImpl>(new ClientPlayerTeamServiceOpenImpl));
    g_player_services.emplace("ServerPlayerTeamService", std::make_unique<ServerPlayerTeamServiceImpl>(new ServerPlayerTeamServiceOpenImpl));
}
