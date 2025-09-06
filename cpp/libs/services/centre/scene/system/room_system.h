#pragma once

#include "core/utils/id/node_id_generator.h"
#include "proto/logic/component/scene_comp.pb.h"
#include <proto/common/common.pb.h>
#include "entt/src/entt/entity/entity.hpp"
#include "core/type_define/type_define.h"
#include "modules/scene/system/room_param.h"

void AddMainRoomToNodeComponent(entt::registry& reg, entt::entity server);

/**
 * @brief The ScenesSystem class handles operations related to scenes in the game.
 */
class RoomSystem
{
public:
    // Constructor and Destructor
    RoomSystem();
    ~RoomSystem();

    // Clear all scenes and related data
    void ClearAllRoomData();

	// 查找已有房间或在最优节点创建房间
	static entt::entity FindOrCreateRoom(uint32_t sceneConfId);

	// 创建房间时根据策略选节点
	static entt::entity SelectBestNodeForRoom(uint32_t sceneConfId);


    static NodeId GetGameNodeIdFromRoomEntity(entt::entity room);

    static void HandleDestroyRoomNode(entt::entity node);

    static void EnterDefaultRoom(const EnterDefaultRoomParam& param);

    static void CompelPlayerChangeRoom(const CompelChangeRoomParam& param);

    static void ReplaceCrashRoomNode(entt::entity crash_node, entt::entity dest_node);

};
