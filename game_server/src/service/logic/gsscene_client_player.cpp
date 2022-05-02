#include "gsscene_client_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ClientPlayerSceneServiceImpl::EnterSceneBySceneIdC2S(EntityPtr& entity,
    const ::EnterSeceneBySceneIdC2SRequest* request,
    ::EnterSeceneBySceneIdC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE EnterSceneBySceneIdC2S
///<<< END WRITING YOUR CODE EnterSceneBySceneIdC2S
}

///<<<rpc end
void ClientPlayerSceneServiceImpl::EnterSceneBySceneConfigIdC2S(EntityPtr& entity,
    const ::EnterSeceneByConfigIdC2SRequest* request,
    ::EnterSeceneByConfigIdC2SResponse* response)
{
///<<< BEGIN WRITING YOUR CODE EnterSceneBySceneConfigIdC2S
///<<< END WRITING YOUR CODE EnterSceneBySceneConfigIdC2S
}

///<<<rpc end
void ClientPlayerSceneServiceImpl::PushEnterSceneS2C(EntityPtr& entity,
    const ::EnterSeceneS2C* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE PushEnterSceneS2C
///<<< END WRITING YOUR CODE PushEnterSceneS2C
}

///<<<rpc end
void ClientPlayerSceneServiceImpl::PushSceneInfoS2C(EntityPtr& entity,
    const ::SceneInfoS2C* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE PushSceneInfoS2C
///<<< END WRITING YOUR CODE PushSceneInfoS2C
}

///<<<rpc end
