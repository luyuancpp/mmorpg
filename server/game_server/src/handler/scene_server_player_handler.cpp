#include "scene_server_player_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/system/player_common_system.h"
#include "src/system/player_scene_system.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/pb/pbc/scene_client_player_service.h"

#include "client_player_proto/scene_client_player.pb.h"
///<<< END WRITING YOUR CODE
void ServerPlayerSceneServiceHandler::EnterSceneGs2Controller(entt::entity player,
	const ::Gs2ControllerEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceHandler::EnterSceneController2Gs(entt::entity player,
	const ::Controller2GsEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
		//todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
	PlayerSceneSystem::EnterScene(player, request->scene_id());
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceHandler::LeaveSceneGs2Controller(entt::entity player,
	const ::Gs2ControllerLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE

///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceHandler::LeaveSceneController2Gs(entt::entity player,
	const ::Controller2GsLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	PlayerSceneSystem::LeaveScene(player);
	if (request->change_gs())//存储完毕以后才能换场景，防止回档
	{
		//离开gs 清除session
		PlayerCommonSystem::RemovePlayerSession(player);
		PlayerCommonSystem::SavePlayer(player);
	}
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceHandler::Gs2ControllerLeaveSceneAsyncSavePlayerComplete(entt::entity player,
	const ::Gs2ControllerLeaveSceneAsyncSavePlayerCompleteRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceHandler::Controller2GsEnterSceneS2C(entt::entity player,
	const ::EnterSceneS2CRequest* request,
	::EnterScenerS2CResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
    auto try_scene = tls.registry.try_get<SceneEntity>(player);
    if (nullptr == try_scene)
    {
        LOG_ERROR << " player not enter scene ";
        return;
    }
    EnterSeceneS2C message;
    message.mutable_scene_info()->CopyFrom(tls.registry.get<SceneInfo>(try_scene->scene_entity_));
    Send2Player(ClientPlayerSceneServicePushEnterSceneS2CMsgId, message, player);
///<<< END WRITING YOUR CODE
}

