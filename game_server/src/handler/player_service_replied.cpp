#include <memory>
#include <unordered_map>
#include "player_service_replied.h"
#include "logic_proto/common_server_player.pb.h"
#include "src/handler/common_server_player_replied.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/handler/scene_server_player_replied.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/handler/team_server_player_replied.h"
std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replieds;
class ServerPlayerLoginServiceRepliedRegisterImpl : public ServerPlayerLoginService{};
class ServerPlayerSceneServiceRepliedRegisterImpl : public ServerPlayerSceneService{};
class ServerPlayerTeamServiceRepliedRegisterImpl : public ServerPlayerTeamService{};
void InitPlayerServcieReplied()
{
    g_player_service_replieds.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceRepliedImpl>(new ServerPlayerLoginServiceRepliedRegisterImpl));
    g_player_service_replieds.emplace("ServerPlayerSceneService", std::make_unique<ServerPlayerSceneServiceRepliedImpl>(new ServerPlayerSceneServiceRepliedRegisterImpl));
    g_player_service_replieds.emplace("ServerPlayerTeamService", std::make_unique<ServerPlayerTeamServiceRepliedImpl>(new ServerPlayerTeamServiceRepliedRegisterImpl));
}
