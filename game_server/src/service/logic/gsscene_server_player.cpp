#include "gsscene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceImpl::LoginMs2Gs(EntityPtr& entity,
    const ::Ms2GsLoginRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE LoginMs2Gs
///<<< END WRITING YOUR CODE LoginMs2Gs
}

void ServerPlayerSceneServiceImpl::LoginGs2Ms(EntityPtr& entity,
    const ::Gs2MsLoginRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE LoginGs2Ms
///<<< END WRITING YOUR CODE LoginGs2Ms
}

void ServerPlayerSceneServiceImpl::ReconnectMs2Gs(EntityPtr& entity,
    const ::Ms2GsReconnectRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE ReconnectMs2Gs
///<<< END WRITING YOUR CODE ReconnectMs2Gs
}

///<<<rpc end
