#pragma once

#include "scene/comp/node_scene_comp.h"
#include "scene/system/node_scene_system.h"
#include "util/node_id_generator.h"
#include "proto/logic/component/scene_comp.pb.h"

struct EnterSceneParam
{
    inline bool CheckValid() const
    {
        return scene != entt::null && enter != entt::null;
    }

    entt::entity scene{ entt::null };
    entt::entity enter{ entt::null };
};

struct EnterDefaultSceneParam
{
    inline bool CheckValid() const
    {
        return enter != entt::null;
    }

    entt::entity enter{ entt::null };
};

struct LeaveSceneParam
{
    inline bool CheckValid() const
    {
        return leaver != entt::null;
    }

    entt::entity leaver{ entt::null };
};

struct CreateGameNodeSceneParam
{
    inline bool CheckValid() const
    {
        return node != entt::null;
    }

    entt::entity node{ entt::null };
    SceneInfoPBComponent sceneInfo;
};

struct DestroySceneParam
{
    inline bool CheckValid() const
    {
        return node != entt::null && scene != entt::null;
    }

    entt::entity node{ entt::null };
    entt::entity scene{ entt::null };
};

void AddMainSceneNodeComponent(entt::registry& reg, entt::entity server);

/**
 * @brief The ScenesSystem class handles operations related to scenes in the game.
 */
class SceneUtil
{
public:
    // Constructor and Destructor
    SceneUtil();
    ~SceneUtil();

    // Clear all scenes and related data
    void Clear();

    // Static methods

    /**
     * @brief GetGameNodeId calculates the Game Node ID from a scene ID.
     * @param scene_id The scene ID to convert.
     * @return The corresponding Game Node ID.
     */
    static NodeId GetGameNodeIdFromGuid(uint64_t scene_id);

    /**
     * @brief GetGameNodeId retrieves the Game Node ID associated with a scene entity.
     * @param scene The entity ID of the scene.
     * @return The Game Node ID.
     */
    static NodeId GetGameNodeIdFromSceneEntity(entt::entity scene);

    /**
     * @brief get_game_node_eid retrieves the entity ID of a game node based on its scene ID.
     * @param scene_id The scene ID of the game node.
     * @return The entity ID of the game node.
     */
    static entt::entity get_game_node_eid(uint64_t scene_id);

    /**
     * @brief GenSceneGuid generates a unique scene GUID.
     * @return The generated scene GUID.
     */
    static uint32_t GenSceneGuid();

    /**
     * @brief GetScenesSize retrieves the number of scenes for a specific scene configuration ID.
     * @param scene_config_id The ID of the scene configuration.
     * @return The number of scenes.
     */
    static std::size_t GetScenesSize(uint32_t scene_config_id);

    /**
     * @brief GetScenesSize retrieves the total number of scenes.
     * @return The total number of scenes.
     */
    static std::size_t GetScenesSize();

    /**
     * @brief SetSequenceNodeId sets the node ID sequence for generating node IDs.
     * @param node_id The starting node ID.
     */
    static void SetSequenceNodeId(const uint32_t node_id);

    /**
     * @brief CheckScenePlayerSize checks the current number of players in a scene.
     * @param scene The entity ID of the scene to check.
     * @return The number of players in the scene.
     */
    static uint32_t CheckScenePlayerSize(entt::entity scene);

    /**
     * @brief CreateScene2GameNode creates a scene associated with a game node.
     * @param param Parameters for creating the scene.
     * @return The entity ID of the created scene.
     */
    static entt::entity CreateSceneToSceneNode(const CreateGameNodeSceneParam& param);

    /**
     * @brief DestroyScene destroys a scene.
     * @param param Parameters for destroying the scene.
     */
    static void DestroyScene(const DestroySceneParam& param);

    /**
     * @brief OnDestroyServer handles actions when a server node is destroyed.
     * @param node The entity ID of the server node.
     */
    static void HandleDestroyGameNode(entt::entity node);

    /**
     * @brief CheckPlayerEnterScene checks if a player can enter a scene.
     * @param param Parameters for checking player entry.
     * @return The result of the check (e.g., success or failure reason).
     */
    static uint32_t CheckPlayerEnterScene(const EnterSceneParam& param);

    /**
     * @brief EnterScene handles the action of a player entering a scene.
     * @param param Parameters for player entry.
     */
    static void EnterScene(const EnterSceneParam& param);

    /**
     * @brief EnterDefaultScene handles the action of a player entering the default scene.
     * @param param Parameters for entering the default scene.
     */
    static void EnterDefaultScene(const EnterDefaultSceneParam& param);

    /**
     * @brief LeaveScene handles the action of a player leaving a scene.
     * @param param Parameters for player leaving.
     */
    static void LeaveScene(const LeaveSceneParam& param);

    /**
     * @brief CompelPlayerChangeScene forces a player to change scenes.
     * @param param Parameters for forcing player scene change.
     */
    static void CompelPlayerChangeScene(const CompelChangeSceneParam& param);

    /**
     * @brief ReplaceCrashServer handles replacing a crashed server node with a destination node.
     * @param crash_node The entity ID of the crashed server node.
     * @param dest_node The entity ID of the destination node.
     */
    static void ReplaceCrashGameNode(entt::entity crash_node, entt::entity dest_node);

    // Test-related methods

    /**
     * @brief ConfigSceneListNotEmpty checks if the scene list for a scene configuration is not empty.
     * @param scene_config_id The ID of the scene configuration.
     * @return True if the scene list is not empty, false otherwise.
     */
    static bool ConfigSceneListNotEmpty(uint32_t scene_config_id);

    /**
     * @brief IsSceneEmpty checks if there are any scenes currently active.
     * @return True if no scenes are active, false otherwise.
     */
    static bool IsSceneEmpty();

};
