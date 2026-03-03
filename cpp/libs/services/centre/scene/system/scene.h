#pragma once

#include "core/utils/id/node_id_generator.h"
#include "proto/common/component/scene_comp.pb.h"
#include <proto/common/base/common.pb.h>
#include "entt/src/entt/entity/entity.hpp"
#include "core/type_define/type_define.h"
#include "modules/scene/system/scene_param.h"

void AddMainSceneToNodeComponent(entt::registry& reg, entt::entity server);

/**
 * @brief The ScenesSystem class handles operations related to scenes in the game.
 */
class SceneSystem
{
public:
    // Constructor and Destructor
    SceneSystem();
    ~SceneSystem();

    // Clear all scenes and related data
    void ClearAllSceneData();

	// Find or create a scene
	static entt::entity FindOrCreateScene(uint32_t sceneConfId);

	static NodeId GetGameNodeIdFromSceneEntity(entt::entity scene);

    static void HandleDestroySceneNode(entt::entity node);

    static void EnterDefaultScene(const EnterDefaultSceneParam& param);

    static void CompelPlayerChangeScene(const CompelChangeSceneParam& param);

    static void ReplaceCrashSceneNode(entt::entity crash_node, entt::entity dest_node);

};
