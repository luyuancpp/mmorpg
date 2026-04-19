
#include "player_lifecycle_handler.h"

///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"
#include "player/system/player_lifecycle.h"
#include <thread_context/registry_manager.h>
///<<< END WRITING YOUR CODE

void ScenePlayerHandler::GateLoginNotify(entt::entity player,const ::GateLoginNotifyRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	// Deprecated: Gate now sends PlayerEnterGameNode RPC directly (carries session_id,
	// enter_gs_type, scene_id). This handler is kept for backward compatibility but
	// should not be reached in normal operation.
	LOG_WARN << "GateLoginNotify: deprecated path reached for player "
	         << entt::to_integral(player)
	         << ", enter_gs_type=" << request->enter_gs_type();
	///<<< END WRITING YOUR CODE

}

void ScenePlayerHandler::ExitGame(entt::entity player,const ::GameNodeExitGameRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "ExitGame: Received player exit request. Player entity = " << entt::to_integral(player)
			 << ", playerId = " << entt::to_integral(player);

	PlayerLifecycleSystem::HandleExitGameNode(player);

	///<<< END WRITING YOUR CODE

}
