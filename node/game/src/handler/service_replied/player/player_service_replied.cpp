#include <memory>
#include <unordered_map>
#include "player_service_replied.h"

#include "centre_server_player_scene_replied_handler.h"
#include "centre_server_player_replied_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;

class CentrePlayerSceneServiceImpl : public CentrePlayerSceneService{};
class CentrePlayerServiceImpl : public CentrePlayerService{};
void InitPlayerServiceReplied()
{
	g_player_service_replied.emplace("CentrePlayerSceneService", std::make_unique<CentrePlayerSceneServiceRepliedHandler>(new CentrePlayerSceneServiceImpl));
	g_player_service_replied.emplace("CentrePlayerService", std::make_unique<CentrePlayerServiceRepliedHandler>(new CentrePlayerServiceImpl));
}