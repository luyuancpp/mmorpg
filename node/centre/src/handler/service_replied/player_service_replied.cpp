#include <memory>
#include <unordered_map>
#include "player_service_replied.h"

#include "common_player_replied_handler.h"
#include "scene_player_replied_handler.h"
#include "game_scene_server_player_replied_handler.h"
#include "game_server_player_replied_handler.h"
std::unordered_map<std::string, std::unique_ptr<PlayerServiceReplied>> g_player_service_replied;

class ClientPlayerCommonServiceImpl : public ClientPlayerCommonService{};
class ClientPlayerSceneServiceImpl : public ClientPlayerSceneService{};
class GamePlayerSceneServiceImpl : public GamePlayerSceneService{};
class GamePlayerServiceImpl : public GamePlayerService{};
void InitPlayerServiceReplied()
{
	g_player_service_replied.emplace("ClientPlayerCommonService", std::make_unique<ClientPlayerCommonServiceRepliedHandler>(new ClientPlayerCommonServiceImpl));
	g_player_service_replied.emplace("ClientPlayerSceneService", std::make_unique<ClientPlayerSceneServiceRepliedHandler>(new ClientPlayerSceneServiceImpl));
	g_player_service_replied.emplace("GamePlayerSceneService", std::make_unique<GamePlayerSceneServiceRepliedHandler>(new GamePlayerSceneServiceImpl));
	g_player_service_replied.emplace("GamePlayerService", std::make_unique<GamePlayerServiceRepliedHandler>(new GamePlayerServiceImpl));
}