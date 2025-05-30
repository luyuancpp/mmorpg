
#include <unordered_map>
#include <memory>
#include <google/protobuf/service.h>
#include "player_state_attribute_sync_handler.h"


#include "game_scene_handler.h"


#include "game_service_handler.h"



extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void InitServiceHandler()
{
	gNodeService.emplace("EntitySyncService", std::make_unique_for_overwrite<EntitySyncServiceHandler>());

	gNodeService.emplace("GameSceneService", std::make_unique_for_overwrite<GameSceneServiceHandler>());

	gNodeService.emplace("GameService", std::make_unique_for_overwrite<GameServiceHandler>());

}
