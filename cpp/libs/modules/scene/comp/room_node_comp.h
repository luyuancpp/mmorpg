
#pragma once

#include <ranges>

#include "engine/core/type_define/type_define.h"
#include "engine/core/node/constants/node_constants.h"

#include "proto/common/common.pb.h"
#include "engine/threading/registry_manager.h"
#include "proto/logic/component/game_node_comp.pb.h"

using SceneList = EntityUnorderedSet;
using RoomList = std::unordered_map<uint32_t, SceneList>;
using RoomPlayers = EntityUnorderedSet; // 弱引用，要解除玩家和场景的耦合


struct MainRoomNode
{
};

struct CrossMainSceneNode
{
};

struct RoomSceneNode
{
};

struct CrossRoomSceneNode
{
};

class NodeRoomComp
{
public:
	[[nodiscard]] const RoomList& GetSceneLists() const
	{
		return configSceneLists;
	}

	std::size_t GetTotalSceneCount() const
	{
		std::size_t totalSize = 0;
		for (const auto& scenes : configSceneLists | std::views::values)
		{
			totalSize += scenes.size();
		}
		return totalSize;
	}

	[[nodiscard]] const SceneList& GetScenesByConfig(uint32_t scene_config_id) const
	{
		const auto it = configSceneLists.find(scene_config_id);
		if (it == configSceneLists.end())
		{
			static const SceneList emptyList;
			return emptyList;
		}
		return it->second;
	}

	void AddRoom(entt::entity scene_id)
	{
		const auto& sceneInfo = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(scene_id);
		configSceneLists[sceneInfo.scene_confid()].emplace(scene_id);
	}

	void RemoveRoom(entt::entity scene_eid)
	{
		const auto& sceneInfo = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(scene_eid);
		auto it = configSceneLists.find(sceneInfo.scene_confid());
		if (it != configSceneLists.end())
		{
			it->second.erase(scene_eid);
		}
		if (tlsRegistryManager.roomRegistry.valid(scene_eid))
		{
			Destroy(tlsRegistryManager.roomRegistry, scene_eid);
		}
	}

	[[nodiscard]] entt::entity GetSceneWithMinPlayerCountByConfigId(uint32_t scene_config_id) const
	{
		const auto& sceneList = GetScenesByConfig(scene_config_id);
		if (sceneList.empty())
		{
			return entt::null;
		}

		entt::entity minPlayerScene = entt::null;
		std::size_t minPlayers = UINT64_MAX;

		for (auto scene : sceneList)
		{
			const auto playerSize = tlsRegistryManager.roomRegistry.get<RoomPlayers>(scene).size();
			if (playerSize >= kMaxScenePlayerSize) // 可以避免重复的大小比较
			{
				continue;
			}
			if (playerSize < minPlayers)
			{
				minPlayers = playerSize;
				minPlayerScene = scene;

				if (playerSize == 0) // 如果已经找到人数为0的场景，可以直接返回，因为不可能有更小的人数
				{
					return minPlayerScene;
				}
			}
		}
		return minPlayerScene;
	}

	void SetNodePressureState(NodePressureState state)
	{
		nodePressureState = state;
	}

	[[nodiscard]] NodePressureState GetNodePressureState() const
	{
		return nodePressureState;
	}

	bool IsNodeNoPressure() const
	{
		return nodePressureState == NodePressureState::kNoPressure;
	}

	bool IsNodePressure() const
	{
		return nodePressureState == NodePressureState::kPressure;
	}

private:
	RoomList configSceneLists;
	NodePressureState nodePressureState{ NodePressureState::kNoPressure };
};


struct NodeStateComp {
	// 加上这个就能直接用枚举比较
	operator NodeState() const {
		return state;
	}

	NodeState state = NodeState::kNormal;
};

using RoomNodePlayerStatsPtrPbComponent = std::shared_ptr<GameNodePlayerInfoPBComponent>;