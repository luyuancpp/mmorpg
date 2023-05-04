#include "common_server_player_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "component_proto/player_login_comp.pb.h"
#include "src/system/player_common_system.h"
///<<< END WRITING YOUR CODE
void ServerPlayerLoginServiceHandler::Controller2GsLogin(entt::entity player,
	const ::Controller2GsLoginRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	if (request->enter_gs_type() == LOGIN_NONE)//登录，不是普通切换场景
	{
		return;
	}
	PlayerCommonSystem::OnPlayerLogin(player, request->enter_gs_type());
///<<< END WRITING YOUR CODE
}
