#include "scene_server_player_replied.h"
#include "src/util/game_registry.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/system/player_scene_system.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceRepliedImpl::EnterSceneGs2Controller(entt::entity player,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceRepliedImpl::EnterSceneController2Gs(entt::entity player,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceRepliedImpl::LeaveSceneGs2Controller(entt::entity player,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceRepliedImpl::LeaveSceneController2Gs(entt::entity player,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceRepliedImpl::Gs2ControllerLeaveSceneAsyncSavePlayerComplete(entt::entity player,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceRepliedImpl::Controller2GsEnterSceneS2C(entt::entity player,
    ::EnterScenerS2CResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
    PlayerSceneSystem::TryEnterNextScene(player);
///<<< END WRITING YOUR CODE
}

///<<<rpc end
