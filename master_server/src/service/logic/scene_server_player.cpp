#include "scene_server_player.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/network/message_sys.hpp"
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
    serverplayer::Ms2GsLoginRequest message;
    Send2GsPlayer(message, entity);
///<<< END WRITING YOUR CODE LogincMs2Gs
}

///<<<rpc end
void PlayerPlayerSceneServiceImpl::ReconnectMs2Gs(common::EntityPtr& entity,
    const serverplayer::Ms2GsReconnectRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE ReconnectMs2Gs
///<<< END WRITING YOUR CODE ReconnectMs2Gs
}

///<<<rpc end
}// namespace serverplayer
