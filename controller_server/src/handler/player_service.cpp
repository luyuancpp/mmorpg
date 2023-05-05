#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/common_server_player.pb.h"
#include "src/service/common_server_player_handler.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/service/scene_server_player_handler.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/service/team_server_player_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
class ServerPlayerLoginServiceOpenImpl : public ServerPlayerLoginService{};
class ServerPlayerSceneServiceOpenImpl : public ServerPlayerSceneService{};
class ServerPlayerTeamServiceOpenImpl : public ServerPlayerTeamService{};
void InitPlayerService()
{
    g_player_services.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceHandler>(new ServerPlayerLoginServiceOpenImpl));
    g_player_services.emplace("ServerPlayerSceneService", std::make_unique<ServerPlayerSceneServiceHandler>(new ServerPlayerSceneServiceOpenImpl));
    g_player_services.emplace("ServerPlayerTeamService", std::make_unique<ServerPlayerTeamServiceHandler>(new ServerPlayerTeamServiceOpenImpl));
}
