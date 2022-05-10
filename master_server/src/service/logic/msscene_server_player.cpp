#include "msscene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceImpl::LoginMs2Gs(entt::entity& player,
    const ::Ms2GsLoginRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::ReconnectMs2Gs(entt::entity& player,
    const ::Ms2GsReconnectRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::EnterSceneGs2Ms(entt::entity& player,
    const ::Gs2MsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<<rpc end
