#include "game_player_handler.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"
#include "game_logic/player/util/player_scene_util.h"
#include "game_logic/player/util/player_node_util.h"
#include "scene/util/scene_util.h"
#include "proto/logic/component/player_login_comp.pb.h"
///<<< END WRITING YOUR CODE
void GamePlayerServiceHandler::Centre2GsLogin(entt::entity player,const ::Centre2GsLoginRequest* request,
	     google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	LOG_INFO << "Handling Centre2GsLoginRequest for player: " << tls.registry.get<Guid>(player) << ", enter_gs_type: " << request->enter_gs_type();

	if (request->enter_gs_type() == LOGIN_NONE) // 登录，不是普通切换场景
	{
		return;
	}

	PlayerNodeUtil::OnPlayerLogin(player, request->enter_gs_type());
	///<<< END WRITING YOUR CODE
}

void GamePlayerServiceHandler::ExitGame(entt::entity player,const ::GameNodeExitGameRequest* request,
	     google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	PlayerSceneUtil::HandleLeaveScene(player);
	SceneUtil::LeaveScene({ .leaver = player });

	PlayerNodeUtil::HandleExitGameNode(player);

	LOG_INFO << "Player " << tls.registry.get<Guid>(player) << " session cleared after exit game.";

///<<< END WRITING YOUR CODE
}

