
#pragma once

#include <ranges>

#include "type_define/type_define.h"
#include "node/constants/node_constants.h"

#include "proto/common/common.pb.h"
#include <base/threading/registry_manager.h>

using SceneList = EntityUnorderedSet;
using ConfigSceneListType = std::unordered_map<uint32_t, SceneList>;
using ScenePlayers = EntityUnorderedSet; // 弱引用，要解除玩家和场景的耦合

entt::entity GlobalEntity();

struct MainSceneNode
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

class NodeSceneComp
{
public:
	[[nodiscard]] const ConfigSceneListType& GetSceneLists() const
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

	void AddScene(entt::entity scene_id)
	{
		const auto& sceneInfo = tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(scene_id);
		configSceneLists[sceneInfo.scene_confid()].emplace(scene_id);
	}

	void RemoveScene(entt::entity scene_eid)
	{
		const auto& sceneInfo = tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(scene_eid);
		auto it = configSceneLists.find(sceneInfo.scene_confid());
		if (it != configSceneLists.end())
		{
			it->second.erase(scene_eid);
		}
		if (tlsRegistryManager.sceneRegistry.valid(scene_eid))
		{
			Destroy(tlsRegistryManager.sceneRegistry, scene_eid);
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
			const auto playerSize = tlsRegistryManager.sceneRegistry.get<ScenePlayers>(scene).size();
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


	void SetState(NodeState state)
	{
		nodeState = state;
	}

	[[nodiscard]] NodeState GetNodeState() const
	{
		return nodeState;
	}

	bool IsStateNormal() const
	{
		return nodeState == NodeState::kNormal;
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
	ConfigSceneListType configSceneLists;
	NodeState nodeState{ NodeState::kNormal };
	NodePressureState nodePressureState{ NodePressureState::kNoPressure };
};

