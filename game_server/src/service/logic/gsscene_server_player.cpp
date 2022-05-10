#include "gsscene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/network/message_sys.h"
#include "src/game_logic/scene/scene.h"

#include "logic_proto/scene_client_player.pb.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceImpl::LoginMs2Gs(entt::entity& player,
    const ::Ms2GsLoginRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    //todo进入了gate 然后才可以开始可以给客户端发送信息了,gs消息顺序问题要注意，进入a,再进入b gs到达客户端消息的顺序不一样
     //顶号，或者登录
    EnterSeceneS2C message;
    auto scene = reg.get<SceneEntity>(player).scene_entity();
    message.mutable_scene_info()->CopyFrom(reg.get<SceneInfo>(scene));
    Send2Player(message, player);
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
