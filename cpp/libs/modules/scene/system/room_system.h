#pragma once

#include "modules/scene/system/node_room_system.h"
#include "core/utils/id/node_id_generator.h"
#include "proto/logic/component/scene_comp.pb.h"
#include <proto/common/common.pb.h>

struct EnterDefaultRoomParam
{
    inline bool CheckValid() const
    {
        return enter != entt::null;
    }

    entt::entity enter{ entt::null };
};

struct CreateRoomOnNodeRoomParam
{
    inline bool CheckValid() const
    {
        return node != entt::null;
    }

    entt::entity node{ entt::null };
    RoomInfoPBComponent roomInfo;
};


struct CompelChangeRoomParam
{
	inline bool IsNull() const
	{
		return player == entt::null || destNode == entt::null;
	}

	entt::entity player{ entt::null };
	entt::entity destNode{ entt::null };
	uint32_t sceneConfId{ 0 };
};

void AddMainRoomToNodeComponent(entt::registry& reg, entt::entity server);

/**
 * @brief The ScenesSystem class handles operations related to scenes in the game.
 */
class RoomUtil
{
public:
    // Constructor and Destructor
    RoomUtil();
    ~RoomUtil();

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
