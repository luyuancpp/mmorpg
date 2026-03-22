#pragma once

#include "entt/src/entt/entity/registry.hpp"

#include "engine/core/type_define/type_define.h"

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


// Cross-zone transfer: reject if target zone is full
// Cross-zone requests should be routed to the target zone first
// Intra-server scene changes don't need cross-zone leave notification
// TODO: Mirror enter/exit must stay on the same GS; main-world sharding is independent of mirrors (saves a full player transfer)
// TODO: Handle reconnect during cross-zone transfer
// TODO: Cross-zone return must notify leave; handle case where origin server is full on return