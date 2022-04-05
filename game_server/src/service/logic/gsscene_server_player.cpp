#include "gsscene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
#include "logic_proto/scene_client_player.pb.h"
///<<< END WRITING YOUR CODE

namespace serverplayer{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceImpl::LoginMs2Gs(common::EntityPtr& entity,
    const serverplayer::Ms2GsLoginRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE LoginMs2Gs
    clientplayer::EnterSeceneS2C message;
    Send2Player(message, entity);
///<<< END WRITING YOUR CODE LoginMs2Gs
}

///<<<rpc end
void ServerPlayerSceneServiceImpl::LoginGs2Ms(common::EntityPtr& entity,
    const serverplayer::Gs2MsLoginRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE LoginGs2Ms
///<<< END WRITING YOUR CODE LoginGs2Ms
}

///<<<rpc end
void ServerPlayerSceneServiceImpl::ReconnectMs2Gs(common::EntityPtr& entity,
    const serverplayer::Ms2GsReconnectRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE ReconnectMs2Gs
///<<< END WRITING YOUR CODE ReconnectMs2Gs
}

///<<<rpc end
}// namespace serverplayer
