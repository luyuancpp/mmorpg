#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "base/core/type_define/type_define.h"

class PlayerSceneContextPBComponent;
class SceneInfoPBComponent;
class ChangeSceneInfoPBComponent;

class PlayerSceneSystem
{
public:
    static void HandleLoginEnterScene(entt::entity player);
    static void SendToGameNodeEnterScene(entt::entity player);
    static void ProcessPlayerEnterSceneNode(entt::entity player, NodeId node_id);
    static void AttemptEnterNextScene(entt::entity player);
    static uint32_t GetDefaultSceneConfigurationId();
    static entt::entity FindSceneForPlayerLogin(const PlayerSceneContextPBComponent& sceneContext);
    static void ProcessEnterGameNode(entt::entity playerEntity, entt::entity sceneEntity);
    static void PushInitialChangeSceneInfo(entt::entity playerEntity, entt::entity sceneEntity);
    static bool VerifyChangeSceneRequest(entt::entity playerEntity);
    static entt::entity ResolveTargetScene(entt::entity playerEntity);
    static bool ValidateSceneSwitch(entt::entity playerEntity, entt::entity toScene);
    static void ProcessSceneChange(entt::entity playerEntity, entt::entity toScene);
    static void HandleEnterScene(entt::entity playerEntity, const SceneInfoPBComponent& sceneInfo);
};


//跨服间切换,如果另一个跨服满了就不应该进去了
//如果是跨服，就应该先跨服去处理
//原来服务器之间换场景，不用通知跨服离开场景
//todo 如果是进出镜像，一定保持在原来的gs切换,主世界分线和镜像没关系，这样就节省了玩家切换流程，效率也提高了
//todo 跨服的时候重新上线
 //跨服到原来服务器，通知跨服离开场景，todo注意回到原来服务器的时候可能原来服务器满了