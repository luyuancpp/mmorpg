#include <memory>
#include <unordered_map>
#include "player_service_replied.h"
#include "logic_proto/common_client_player.pb.h"
#include "src/service/logic_proto_replied/common_client_player_replied.h"
#include "logic_proto/common_server_player.pb.h"
#include "src/service/logic_proto_replied/common_server_player_replied.h"
#include "logic_proto/scene_client_player.pb.h"
#include "src/service/logic_proto_replied/scene_client_player_replied.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/service/logic_proto_replied/scene_server_player_replied.h"
#include "logic_proto/team_client_player.pb.h"
#include "src/service/logic_proto_replied/team_client_player_replied.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/service/logic_proto_replied/team_server_player_replied.h"
std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replieds;
class ClientPlayerCommonServiceRepliedRegisterImpl : public ClientPlayerCommonService{};
class ServerPlayerLoginServiceRepliedRegisterImpl : public ServerPlayerLoginService{};
class ClientPlayerSceneServiceRepliedRegisterImpl : public ClientPlayerSceneService{};
class ServerPlayerSceneServiceRepliedRegisterImpl : public ServerPlayerSceneService{};
class ClientPlayerTeamServiceRepliedRegisterImpl : public ClientPlayerTeamService{};
class ServerPlayerTeamServiceRepliedRegisterImpl : public ServerPlayerTeamService{};
void InitPlayerServcieReplied()
{
    g_player_service_replieds.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceRepliedImpl>(new ClientPlayerCommonServiceRepliedRegisterImpl));
    g_player_service_replieds.emplace("ServerPlayerLoginService", std::make_unique<ServerPlayerLoginServiceRepliedImpl>(new ServerPlayerLoginServiceRepliedRegisterImpl));
    g_player_service_replieds.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceRepliedImpl>(new ClientPlayerSceneServiceRepliedRegisterImpl));
    g_player_service_replieds.emplace("ServerPlayerSceneService", std::make_unique<ServerPlayerSceneServiceRepliedImpl>(new ServerPlayerSceneServiceRepliedRegisterImpl));
    g_player_service_replieds.emplace("ClientPlayerTeamService", std::make_unique<ClientPlayerTeamServiceRepliedImpl>(new ClientPlayerTeamServiceRepliedRegisterImpl));
    g_player_service_replieds.emplace("ServerPlayerTeamService", std::make_unique<ServerPlayerTeamServiceRepliedImpl>(new ServerPlayerTeamServiceRepliedRegisterImpl));
}
