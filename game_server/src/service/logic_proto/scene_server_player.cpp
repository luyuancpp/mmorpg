#include "scene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/system/player_common_system.h"
#include "src/system/player_scene_system.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceImpl::EnterSceneGs2Controller(entt::entity player,
    const ::Gs2ControllerEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::EnterSceneController2Gs(entt::entity player,
    const ::Controller2GsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
		//todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
	PlayerSceneSystem::EnterScene(player, request->scene_id());
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::LeaveSceneGs2Controller(entt::entity player,
    const ::Gs2ControllerLeaveSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::LeaveSceneController2Gs(entt::entity player,
    const ::Controller2GsLeaveSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	PlayerSceneSystem::LeaveScene(player);
	if (request->change_gs())//存储完毕以后才能换场景，防止回档
	{
		//离开gs 清除session
		PlayerCommonSystem::RemovePlayereSession(player);
		PlayerCommonSystem::SavePlayer(player);
	}
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::Gs2ControllerLeaveSceneAsyncSavePlayerComplete(entt::entity player,
    const ::Gs2ControllerLeaveSceneAsyncSavePlayerCompleteRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<<rpc end
