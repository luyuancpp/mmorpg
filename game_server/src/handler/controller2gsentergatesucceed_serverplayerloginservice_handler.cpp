#include "common_server_player_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/system/player_common_system.h"
///<<< END WRITING YOUR CODE
void ServerPlayerLoginServiceHandler::Controller2GsEnterGateSucceed(entt::entity player,
	const ::Controller2GsEnterGateSucceedRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	PlayerCommonSystem::OnEnterGateSucceed(player);
///<<< END WRITING YOUR CODE
}
