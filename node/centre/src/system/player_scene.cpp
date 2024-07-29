#include "player_scene.h"
#include "muduo/base/Logging.h"
#include "centre_node.h"
#include "comp/scene.h"
#include "system/scene/scene_system.h"
#include "constants/tips_id.h"
#include "comp/game_node.h"
#include "network/message_system.h"
#include "service/game_scene_server_player_service.h"
#include "service/game_service_service.h"
#include "system/player_change_scene.h"
#include "system/player_tip.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/logic/component/player_scene_comp.pb.h"

void PlayerSceneSystem::HandleLoginEnterScene(entt::entity playerEntity)
{
    if (!tls.registry.valid(playerEntity))
    {
        LOG_ERROR << "Player entity is not valid";
        return;
    }

    const auto* playerSceneInfo = tls.registry.try_get<PlayerSceneInfoComp>(playerEntity);
    if (!playerSceneInfo)
    {
        LOG_ERROR << "Player scene info not found";
        return;
    }

    entt::entity currentSceneId = entt::null;
    entt::entity lastVisitedSceneId = entt::null;

    // Check if previous scene is valid
    if (tls.sceneRegistry.valid(entt::entity{ playerSceneInfo->scene_info().guid() }))
    {
        if (kOK == ScenesSystem::CheckPlayerEnterScene({ .scene = entt::entity{ playerSceneInfo->scene_info().guid() }, .enter = playerEntity }))
        {
            currentSceneId = entt::entity{ playerSceneInfo->scene_info().guid() };
        }
    }
    else if (tls.sceneRegistry.valid(entt::entity{ playerSceneInfo->scene_info_last_time().guid() }))
    {
        if (kOK == ScenesSystem::CheckPlayerEnterScene({ .scene = entt::entity{ playerSceneInfo->scene_info_last_time().guid() }, .enter = playerEntity }))
        {
            lastVisitedSceneId = entt::entity{ playerSceneInfo->scene_info_last_time().guid() };
        }
    }

    // Find appropriate scene for the player to enter
    if (currentSceneId == entt::null && lastVisitedSceneId == entt::null)
    {
        if (playerSceneInfo->scene_info().scene_confid() > 0)
        {
            currentSceneId = NodeSceneSystem::FindNotFullScene({ playerSceneInfo->scene_info().scene_confid() });
            if (currentSceneId == entt::null)
            {
                currentSceneId = NodeSceneSystem::FindNotFullScene({ playerSceneInfo->scene_info().scene_confid() });
            }
        }
    }

    // If still no valid scene found, fallback to default scene
    if (currentSceneId == entt::null)
    {
        currentSceneId = NodeSceneSystem::FindNotFullScene({ GetDefaultSceneConfigurationId() });
    }

    if (currentSceneId == entt::null)
    {
        LOG_ERROR << "Failed to find a scene for player login";
        return;
    }

    // Call method to handle player entering the game server
    ProcessPlayerEnterGameServer(playerEntity, ScenesSystem::GetGameNodeId(currentSceneId));

    // Prepare change scene information
    CentreChangeSceneInfo changeSceneInfo;
    PlayerChangeSceneSystem::CopySceneInfoToChangeInfo(changeSceneInfo, tls.sceneRegistry.get<SceneInfo>(currentSceneId));
    changeSceneInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
    changeSceneInfo.set_change_gs_status(CentreChangeSceneInfo::eEnterGsSceneSucceed);
    PlayerChangeSceneSystem::PushChangeSceneInfo(playerEntity, changeSceneInfo);
}

void PlayerSceneSystem::SendToGameServerEnterScene(entt::entity playerEntity)
{
    if (playerEntity == entt::null)
    {
        LOG_ERROR << "Player entity is null";
        return;
    }

    const auto* playerSceneEntity = tls.registry.try_get<SceneEntityComp>(playerEntity);
    const auto playerId = tls.registry.get<Guid>(playerEntity);
    if (!playerSceneEntity)
    {
        LOG_ERROR << "Player has not entered a scene: " << playerId;
        return;
    }

    const auto* sceneInfo = tls.sceneRegistry.try_get<SceneInfo>((*playerSceneEntity).sceneEntity);
    if (!sceneInfo)
    {
        LOG_ERROR << "Scene info not found for player: " << playerId;
        return;
    }

    const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(playerEntity);
    if (!playerNodeInfo)
    {
        LOG_ERROR << "Player session not valid for player: " << playerId;
        return;
    }

    Centre2GsEnterSceneRequest request;
    request.set_scene_id(sceneInfo->guid());
    request.set_player_id(playerId);
    CallGameNodeMethod(GameServiceEnterSceneMsgId, request, playerNodeInfo->game_node_id());

    LOG_DEBUG << "Player entered scene: " << playerId << ", Scene ID: " << sceneInfo->guid() << ", Game Node ID: " << playerNodeInfo->game_node_id();
}

void PlayerSceneSystem::ProcessPlayerEnterGameServer(entt::entity playerEntity, NodeId nodeId)
{
    const auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(playerEntity);
    if (!playerNodeInfo)
    {
        LOG_ERROR << "Player session not valid";
        return;
    }

    GameNodeEnterGsRequest request;
    request.set_player_id(tls.registry.get<Guid>(playerEntity));
    request.set_session_id(playerNodeInfo->gate_session_id());
    request.set_centre_node_id(gCentreNode ->GetNodeId());
    CallGameNodeMethod(GameServiceEnterGsMsgId, request, nodeId);
}

void PlayerSceneSystem::AttemptEnterNextScene(entt::entity playerEntity)
{
    auto* changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(playerEntity);
    if (!changeSceneQueue)
    {
        return;
    }

    const auto* fromSceneEntity = tls.registry.try_get<SceneEntityComp>(playerEntity);
    if (!fromSceneEntity)
    {
        PlayerTipSystem::SendToPlayer(playerEntity, kRetEnterSceneYourSceneIsNull, {});
        return;
    }

    auto& changeSceneInfo = changeSceneQueue->changeSceneQueue.front();
    if (changeSceneInfo.processing())
    {
        return;
    }

    changeSceneInfo.set_processing(true);
    auto toSceneGuid = changeSceneInfo.guid();
    entt::entity toScene = entt::null;

    if (toSceneGuid <= 0)
    {
        GetSceneParams getSceneParams;
        getSceneParams.sceneConfigurationId = changeSceneInfo.scene_confid();
        toScene = NodeSceneSystem::FindNotFullScene(getSceneParams);
        if (toScene == entt::null)
        {
            PlayerTipSystem::SendToPlayer(playerEntity, kRetEnterSceneSceneFull, {});
            PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
            return;
        }
        toSceneGuid = tls.registry.get<SceneInfo>(toScene).guid();
    }
    else
    {
        toScene = entt::entity{ toSceneGuid };
        if (toScene == entt::null)
        {
            PlayerTipSystem::SendToPlayer(playerEntity, kRetEnterSceneSceneNotFound, {});
            PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
            return;
        }
    }

    const auto* fromSceneInfo = tls.sceneRegistry.try_get<SceneInfo>(fromSceneEntity->sceneEntity);
    if (!fromSceneInfo)
    {
        return;
    }

    const auto fromSceneGameNode = ScenesSystem::get_game_node_eid(fromSceneInfo->guid());
    const auto toSceneGameNode = ScenesSystem::get_game_node_eid(toSceneGuid);
    if (!tls.gameNodeRegistry.valid(fromSceneGameNode) || !tls.gameNodeRegistry.valid(toSceneGameNode))
    {
        LOG_ERROR << "Scene not found: " << fromSceneInfo->guid() << " to " << toSceneGuid;
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
        return;
    }

    if (toSceneGuid == fromSceneInfo->guid())
    {
        PlayerTipSystem::SendToPlayer(playerEntity, kRetEnterSceneYouInCurrentScene, {});
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
        return;
    }

    if (!changeSceneInfo.ignore_full())
    {
        if (const auto ret = ScenesSystem::CheckScenePlayerSize(toScene); ret != kOK)
        {
            PlayerTipSystem::SendToPlayer(playerEntity, ret, {});
            PlayerChangeSceneSystem::PopFrontChangeSceneQueue(playerEntity);
            return;
        }
    }

    if (fromSceneGameNode == toSceneGameNode)
    {
        changeSceneInfo.set_change_gs_type(CentreChangeSceneInfo::eSameGs);
    }
    else
    {
        changeSceneInfo.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
    }

    PlayerChangeSceneSystem::ProcessChangeSceneQueue(playerEntity);
}

uint32_t PlayerSceneSystem::GetDefaultSceneConfigurationId()
{
    return 1;
}
