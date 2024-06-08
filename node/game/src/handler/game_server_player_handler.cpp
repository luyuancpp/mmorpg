#include "game_server_player_handler.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"

#include "src/thread_local/thread_local_storage_common_logic.h"
#include "src/network/session.h"
#include "src/system/player_common_system.h"
#include "src/thread_local/game_thread_local_storage.h"

#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE
void GamePlayerServiceHandler::Centre2GsLogin(entt::entity player,
	const ::Centre2GsLoginRequest* request,
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

void GamePlayerServiceHandler::OnCentreEnterGateSucceed(entt::entity player,
	const ::Centre2GsEnterGateSucceedRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	PlayerCommonSystem::OnGateUpdateGameNodeSucceed(player);
	///<<< END WRITING YOUR CODE
}
