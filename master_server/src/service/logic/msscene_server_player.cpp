#include "msscene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"

#include "src/game_logic/scene/scene.h"
#include "src/game_logic/scene/servernode_system.h"
#include "src/game_logic/tips_id.h"
#include "src/system/player_scene_system.h"

#include "src/pb/pbc/logic_proto/common_client_player.pb.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceImpl::EnterSceneGs2Ms(entt::entity player,
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
        scene_id = registry.get<Guid>(scene);
    }
    else
    {
        scene = ScenesSystem::GetSingleton().get_scene(scene_id);
        if (entt::null == scene)
        {
            TipsS2C message;
            message.mutable_tips()->set_id(kRetEnterSceneSceneFull);
            Send2Player(message, player);
            return;
        }
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
    
    auto p_scene_gs = registry.try_get<GsDataPtr>(scene);
    auto p_player_gs = registry.try_get<GsDataPtr>(player);
    if (nullptr == p_scene_gs || nullptr == p_player_gs)
    {
        LOG_ERROR << " scene null : " << (nullptr == p_scene_gs) << " " << (nullptr == p_player_gs);
        return;
    }
    auto& scene_gs = *p_scene_gs;
    auto& player_gs = *p_player_gs;
    //同gs之间的切换
    if (player_gs->node_id() == scene_gs->node_id())
    {
        EnterSceneParam ep;
        ep.enterer_ = player;
        ep.scene_ = scene;
        ScenesSystem::GetSingleton().EnterScene(ep);
        PlayerSceneSystem::EnterGsScene(player);
    }
    else
    {

    }

///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::EnterSceneMs2Gs(entt::entity player,
    const ::Ms2GsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<<rpc end
