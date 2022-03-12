#include <memory>
#include <unordered_map>
#include "player_service.h"
#include "c2gs.pb.h"
#include "c2gs_player.h"
namespace game
{
std::unordered_map<std::string, std::unique_ptr<PlayerService>> g_player_services;
class C2GsServiceImpl : public c2gs::C2GsService{};
void InitPlayerServcie()
{
    g_player_services.emplace("c2gs.C2GsService", std::make_unique<c2gs::PlayerC2GsServiceImpl>(new C2GsServiceImpl));
}
}//namespace game
