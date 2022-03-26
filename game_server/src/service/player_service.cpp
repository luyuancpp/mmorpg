#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "logic_proto/ms2gs_scene.pb.h"
#include "ms2gs_scene_player.h"
#include "logic_proto/player_scene.pb.h"
#include "player_scene_player.h"
#include "logic_proto/player_team.pb.h"
#include "player_team_player.h"
namespace game
{
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
class C2GsServiceImpl : public ms2gsscene::C2GsService{};
class C2GsSceneServiceImpl : public playerscene::C2GsSceneService{};
class C2GsTeamServiceImpl : public playerteam::C2GsTeamService{};
void InitPlayerServcie()
{
    g_player_services.emplace("ms2gsscene.C2GsService", std::make_unique<ms2gsscene::PlayerC2GsServiceImpl>(new C2GsServiceImpl));
    g_player_services.emplace("playerscene.C2GsSceneService", std::make_unique<playerscene::PlayerC2GsSceneServiceImpl>(new C2GsSceneServiceImpl));
    g_player_services.emplace("playerteam.C2GsTeamService", std::make_unique<playerteam::PlayerC2GsTeamServiceImpl>(new C2GsTeamServiceImpl));
}
}//namespace game
