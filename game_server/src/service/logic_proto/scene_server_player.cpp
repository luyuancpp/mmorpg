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
		//todo������gate Ȼ��ſ��Կ�ʼ���Ը��ͻ��˷�����Ϣ��, gs��Ϣ˳������Ҫע�⣬����a, �ٽ���b gs����ͻ�����Ϣ��˳��һ��
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
	if (request->change_gs())//�洢����Ժ���ܻ���������ֹ�ص�
	{
		//�뿪gs ���session
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
