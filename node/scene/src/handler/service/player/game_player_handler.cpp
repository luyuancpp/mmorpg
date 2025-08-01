
#include "game_player_handler.h"

///<<< BEGIN WRITING YOUR CODE

#include "player/system/player_node_system.h"
#include "player/system/player_scene_system.h"
#include "muduo/base/Logging.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "scene/system/scene_system.h"
///<<< END WRITING YOUR CODE



void ScenePlayerHandler::Centre2GsLogin(entt::entity player,const ::Centre2GsLoginRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling Centre2GsLoginRequest for player: " << tls.actorRegistry.get<Guid>(player) << ", enter_gs_type: " << request->enter_gs_type();

	if (request->enter_gs_type() == LOGIN_NONE) // 登录，不是普通切换场景
	{
		return;
	}

	PlayerNodeSystem::OnPlayerLogin(player, request->enter_gs_type());
	///<<< END WRITING YOUR CODE

}


void ScenePlayerHandler::ExitGame(entt::entity player,const ::GameNodeExitGameRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "ExitGame: Received player exit request. Player entity = " << entt::to_integral(player)
		<< ", playerId = " << tls.actorRegistry.get<Guid>(player);

	PlayerNodeSystem::HandleExitGameNode(player);

///<<< END WRITING YOUR CODE

}

