
#include "game_player_handler.h"

///<<< BEGIN WRITING YOUR CODE

#include "player/system/player_lifecycle.h"
#include "player/system/player_scene.h"
#include "muduo/base/Logging.h"
#include "proto/common/component/player_login_comp.pb.h"
///<<< END WRITING YOUR CODE

void ScenePlayerHandler::GateLoginNotify(entt::entity player,const ::GateLoginNotifyRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "GateLoginNotify for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player) << ", enter_gs_type: " << request->enter_gs_type();

	if (request->enter_gs_type() == LOGIN_NONE)
	{
		return;
	}

	PlayerLifecycleSystem::OnPlayerLogin(player, request->enter_gs_type());
///<<< END WRITING YOUR CODE

}

void ScenePlayerHandler::ExitGame(entt::entity player,const ::GameNodeExitGameRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "ExitGame: Received player exit request. Player entity = " << entt::to_integral(player)
		<< ", playerId = " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);

	PlayerLifecycleSystem::HandleExitGameNode(player);

///<<< END WRITING YOUR CODE

}
