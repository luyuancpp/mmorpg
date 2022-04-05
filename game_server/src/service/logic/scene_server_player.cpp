#include "scene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/module/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
#include "logic_proto/scene_client_player.pb.h"
///<<< END WRITING YOUR CODE

namespace serverplayer{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void PlayerPlayerSceneServiceImpl::LogincMs2Gs(common::EntityPtr& entity,
    const serverplayer::Ms2GsLoginRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE LogincMs2Gs
    clientplayer::EnterSeceneS2C message;
    Send2Player(message, entity);
///<<< END WRITING YOUR CODE LogincMs2Gs
}

void PlayerPlayerSceneServiceImpl::ReconnectMs2Gs(common::EntityPtr& entity,
    const serverplayer::Ms2GsReconnectRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE LogincMs2Gs
///<<< END WRITING YOUR CODE LogincMs2Gs
}

///<<<rpc end
///<<< BEGIN WRITING YOUR CODE ReconnectMs2Gs
///<<< END WRITING YOUR CODE ReconnectMs2Gs

}// namespace serverplayer
