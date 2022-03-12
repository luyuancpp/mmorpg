#include <memory>
#include "player_service.h"
#include "c2gs_player.h"
std::map<std::string, std::unique_ptr<PlayerService>> g_player_services;
void InitPlayerServcie()
{
    g_player_services.emplace("c2gs.C2GsService", std::make_unique_ptr<PlayerC2GsService>());
}
