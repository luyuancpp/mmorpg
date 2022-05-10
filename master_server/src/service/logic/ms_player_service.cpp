#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/team_server_player.pb.h"
#include "src/service/logic/msteam_server_player.h"
#include "logic_proto/scene_server_player.pb.h"
#include "src/service/logic/msscene_server_player.h"
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
std::unordered_set<std::string> g_open_player_services;
void InitPlayerServcie()
{
}
