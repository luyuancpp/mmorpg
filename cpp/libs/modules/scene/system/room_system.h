#pragma once

#include "modules/scene/system/node_room_system.h"
#include "core/utils/id/node_id_generator.h"
#include "proto/logic/component/scene_comp.pb.h"
#include <proto/common/common.pb.h>

struct EnterRoomParam
{
    inline bool CheckValid() const
    {
        return room != entt::null && enter != entt::null;
    }

    entt::entity room{ entt::null };
    entt::entity enter{ entt::null };
};

struct EnterDefaultRoomParam
{
    inline bool CheckValid() const
    {
        return enter != entt::null;
    }

    entt::entity enter{ entt::null };
};

struct LeaveRoomParam
{
    inline bool CheckValid() const
    {
        return leaver != entt::null;
    }

    entt::entity leaver{ entt::null };
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

struct DestroyRoomParam
{
    inline bool CheckValid() const
    {
        return node != entt::null && room != entt::null;
    }

    entt::entity node{ entt::null };
    entt::entity room{ entt::null };
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

    // Static methods

    /**
     * @brief GetGameNodeId retrieves the Game Node ID associated with a scene entity.
     * @param scene The entity ID of the scene.
     * @return The Game Node ID.
     */
    static NodeId GetGameNodeIdFromRoomEntity(entt::entity room);

    /**
     * @brief CreateScene2GameNode creates a scene associated with a game node.
     * @param param Parameters for creating the scene.
     * @return The entity ID of the created scene.
     */
    static entt::entity CreateRoomOnRoomNode(const CreateRoomOnNodeRoomParam& param);

    /**
     * @brief DestroyScene destroys a scene.
     * @param param Parameters for destroying the scene.
     */
    static void DestroyRoom(const DestroyRoomParam& param);

    /**
     * @brief OnDestroyServer handles actions when a server node is destroyed.
     * @param node The entity ID of the server node.
     */
    static void HandleDestroyRoomNode(entt::entity node);

    /**
     * @brief EnterScene handles the action of a player entering a scene.
     * @param param Parameters for player entry.
     */
    static void EnterRoom(const EnterRoomParam& param);

    /**
     * @brief EnterDefaultScene handles the action of a player entering the default scene.
     * @param param Parameters for entering the default scene.
     */
    static void EnterDefaultRoom(const EnterDefaultRoomParam& param);

    /**
     * @brief CompelPlayerChangeScene forces a player to change scenes.
     * @param param Parameters for forcing player scene change.
     */
    static void CompelPlayerChangeRoom(const CompelChangeRoomParam& param);

    /**
     * @brief ReplaceCrashServer handles replacing a crashed server node with a destination node.
     * @param crash_node The entity ID of the crashed server node.
     * @param dest_node The entity ID of the destination node.
     */
    static void ReplaceCrashRoomNode(entt::entity crash_node, entt::entity dest_node);

    // Test-related methods

    /**
     * @brief IsSceneEmpty checks if there are any scenes currently active.
     * @return True if no scenes are active, false otherwise.
     */
    static bool IsRoomEmpty();

};
