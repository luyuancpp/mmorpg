#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/common_server_player.pb.h"
#include "src/service/logic_proto/common_server_player.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/service/logic_proto/scene_server_player.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/service/logic_proto/team_server_player.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
class ServerPlayerLoginServiceOpenImpl : public ServerPlayerLoginService{};
class ServerPlayerSceneServiceOpenImpl : public ServerPlayerSceneService{};
class ServerPlayerTeamServiceOpenImpl : public ServerPlayerTeamService{};
void InitPlayerServcie()
{
    g_player_services.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceImpl>(new ServerPlayerLoginServiceOpenImpl));
    g_player_services.emplace("ServerPlayerSceneService", std::make_unique<ServerPlayerSceneServiceImpl>(new ServerPlayerSceneServiceOpenImpl));
    g_player_services.emplace("ServerPlayerTeamService", std::make_unique<ServerPlayerTeamServiceImpl>(new ServerPlayerTeamServiceOpenImpl));
}
