#pragma once

#include "comp/node_scene.h"

#include "system/scene/node_scene_system.h"
#include "util/snow_flake.h"
#include "proto/logic/component/scene_comp.pb.h"

struct EnterSceneParam
{
	inline bool CheckValid() const
	{
		return scene == entt::null || enter == entt::null;
	}

	entt::entity scene{entt::null};
	entt::entity enter{entt::null};
};

struct EnterDefaultSceneParam
{
	inline bool CheckValid() const
	{
		return enter == entt::null;
	}

	entt::entity enter{entt::null};
};

struct LeaveSceneParam
{
	inline bool CheckValid() const
	{
		return leaver == entt::null;
	}

	entt::entity leaver{entt::null};
};

struct CreateGameNodeSceneParam
{
	inline bool CheckValid() const
	{
		return node == entt::null;
	}

	entt::entity node{entt::null};
	SceneInfo sceneInfo;
};

struct DestroySceneParam
{
	inline bool CheckValid() const
	{
		return node == entt::null || scene == entt::null;
	}
    entt::entity node{ entt::null };
    entt::entity scene{ entt::null };
};

void AddMainSceneNodeComponent(entt::registry& reg, entt::entity server);


//todo 1线镜像和一线应该是同一个gs,这样就不会有切换服务器的开销
class ScenesSystem
{
public:
	//just for test clear
	ScenesSystem();
	~ScenesSystem();

	static NodeId GetGameNodeId(uint64_t scene_id)
	{
		return nodeSequence.node_id(static_cast<NodeId>(scene_id));
	}

    static NodeId GetGameNodeId(entt::entity scene);

    static entt::entity get_game_node_eid(uint64_t scene_id)
    {
		return entt::entity{ nodeSequence.node_id(static_cast<NodeId>(scene_id)) };
    }

	static uint32_t GenSceneGuid();

	static std::size_t GetScenesSize(uint32_t scene_config_id);
	static std::size_t GetScenesSize();
	static void SetSequenceNodeId(const uint32_t node_id) { nodeSequence.set_node_id(node_id); }

	static uint32_t CheckScenePlayerSize(entt::entity scene);

	static entt::entity CreateScene2GameNode(const CreateGameNodeSceneParam& param);

	static void DestroyScene(const DestroySceneParam& param);
	static void OnDestroyServer(entt::entity node);

	static uint32_t CheckPlayerEnterScene(const EnterSceneParam& param);
	static void EnterScene(const EnterSceneParam& param);
	static void EnterDefaultScene(const EnterDefaultSceneParam& param);

	static void LeaveScene(const LeaveSceneParam& param);

	static void CompelPlayerChangeScene(const CompelChangeSceneParam& param);

	static void ReplaceCrashServer(entt::entity crash_node, entt::entity dest_node);

	//for Test
    static bool ConfigSceneListNotEmpty(uint32_t scene_config_id);
    static bool IsSceneEmpty();
private:
	inline static NodeBit12Sequence nodeSequence;
};
