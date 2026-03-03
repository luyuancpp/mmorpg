#pragma once
#include <core/type_define/type_define.h>

struct EnterSceneParam
{
	inline bool CheckValid() const
	{
		return scene != entt::null && enter != entt::null;
	}

	entt::entity scene{ entt::null };
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

struct DestroySceneParam
{
	inline bool CheckValid() const
	{
		return node != entt::null && scene != entt::null;
	}

	entt::entity node{ entt::null };
	entt::entity scene{ entt::null };
};

struct CreateSceneOnNodeSceneParam;

// SceneCommon.h
class SceneCommon {
public:
	static NodeId GetGameNodeIdFromGuid(uint64_t scene_id);
	static entt::entity GetSceneNodeEntityId(uint64_t scene_id);
	static void SetSequenceNodeId(uint32_t node_id);
	static void ClearAllSceneData();

	static uint32_t GenSceneGuid();

	static bool IsSceneEmpty();
	static std::size_t GetScenesSize();
	static std::size_t GetScenesSize(uint32_t sceneConfigId);
	static bool ConfigSceneListNotEmpty(uint32_t sceneConfigId);

	static uint32_t CheckPlayerEnterScene(const EnterSceneParam& param);
	static uint32_t HasSceneSlot(entt::entity scene);

	static void EnterScene(const EnterSceneParam& param);
	static void LeaveScene(const LeaveSceneParam& param);

	// 销毁房间（只负责本地）
	static void DestroyScene(const DestroySceneParam& param);

	static entt::entity CreateSceneOnSceneNode(const CreateSceneOnNodeSceneParam& param);

};
