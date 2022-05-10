#include "msscene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_sys.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/scene/servernode_sys.h"
#include "src/return_code/error_code.h"

#include "src/pb/pbc/logic_proto/common_client_player.pb.h"
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
    auto scene_id = request->scene_info().scene_id();
    entt::entity scene = entt::null;
    if (scene_id <= 0)//用scene_config id 去换
    {
        GetSceneParam getp;
        getp.scene_confid_ = request->scene_info().scene_confid();
        scene = ServerNodeSystem::GetWeightRoundRobinMainScene(getp);
        if (entt::null == scene)
        {
            TipsS2C message;
            message.mutable_tips()->set_id(kRetEnterSceneSceneFull);
            Send2Player(message, player);
            return;
        }
        scene_id = reg.get<Guid>(scene);
    }
    CheckEnterSceneParam csp;
    csp.scene_id_ = scene_id;
    csp.player_ = player;
    auto ret = ScenesSystem::GetSingleton().CheckEnterSceneByGuid(csp);
    if (kRetOK != ret)
    {
        TipsS2C message;
        message.mutable_tips()->set_id(ret);
        Send2Player(message, player);
        return;
    }
    
///<<< END WRITING YOUR CODE
}

///<<<rpc end
