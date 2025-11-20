
#include "game_player_handler.h"

///<<< BEGIN WRITING YOUR CODE

#include "player/system/player_lifecycle.h"
#include "player/system/player_scene.h"
#include "muduo/base/Logging.h"
#include "proto/logic/component/player_login_comp.pb.h"
///<<< END WRITING YOUR CODE



void ScenePlayerHandler::Centre2GsLogin(entt::entity player,const ::Centre2GsLoginRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling Centre2GsLoginRequest for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player) << ", enter_gs_type: " << request->enter_gs_type();

	if (request->enter_gs_type() == LOGIN_NONE) // 登录，不是普通切换场景
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

