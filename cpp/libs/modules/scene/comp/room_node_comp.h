
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

class RoomRegistryComp {
public:
	// type alias
	using RoomList = std::unordered_map<uint32_t, EntityUnorderedSet>;

	[[nodiscard]] const RoomList& GetRoomMap() const { return roomsByConfigId; }

	std::size_t GetTotalRoomCount() const {
		std::size_t total = 0;
		for (const auto& [_, roomSet] : roomsByConfigId)
			total += roomSet.size();
		return total;
	}

	[[nodiscard]] const EntityUnorderedSet& GetRoomsByConfig(uint32_t configId) const {
		static const EntityUnorderedSet emptySet;
		auto it = roomsByConfigId.find(configId);
		return it != roomsByConfigId.end() ? it->second : emptySet;
	}

	void AddRoom(entt::entity roomEntity) {
		const auto& roomInfo = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(roomEntity);
		roomsByConfigId[roomInfo.scene_confid()].emplace(roomEntity);
	}

	void RemoveRoom(entt::entity roomEntity) {
		const auto& roomInfo = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(roomEntity);
		auto it = roomsByConfigId.find(roomInfo.scene_confid());
		if (it != roomsByConfigId.end()) {
			it->second.erase(roomEntity);
		}

		Destroy(tlsRegistryManager.roomRegistry, roomEntity);
	}

private:
	RoomList roomsByConfigId;
};


class NodeRoomComp
{
public:
	[[nodiscard]] const RoomList& GetRoomMap() const
	{
		return roomsByConfigId;
	}

	std::size_t GetTotalRoomCount() const
	{
		std::size_t totalSize = 0;
		for (const auto& scenes : roomsByConfigId | std::views::values)
		{
			totalSize += scenes.size();
		}
		return totalSize;
	}

	[[nodiscard]] const SceneList& GetRoomsByConfig(uint32_t scene_config_id) const
	{
		const auto it = roomsByConfigId.find(scene_config_id);
		if (it == roomsByConfigId.end())
		{
			static const SceneList emptyList;
			return emptyList;
		}
		return it->second;
	}

	void AddRoom(entt::entity scene_id)
	{
		const auto& sceneInfo = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(scene_id);
		roomsByConfigId[sceneInfo.scene_confid()].emplace(scene_id);
	}

	void RemoveRoom(entt::entity scene_eid)
	{
		const auto& sceneInfo = tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(scene_eid);
		auto it = roomsByConfigId.find(sceneInfo.scene_confid());
		if (it != roomsByConfigId.end())
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
		const auto& sceneList = GetRoomsByConfig(scene_config_id);
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

private:
	RoomList roomsByConfigId;
};


struct NodeStateComp {
	// 加上这个就能直接用枚举比较
	operator NodeState() const {
		return state;
	}

	bool IsNormal() const {
		return state == NodeState::kNormal;
	}

	NodeState state = NodeState::kNormal;
};

struct NodePressureComp {
	operator NodePressureState() const {
		return state;
	}

	// 可选：添加便捷比较
	bool operator==(NodePressureState other) const {
		return state == other;
	}
	bool operator!=(NodePressureState other) const {
		return state != other;
	}

	bool IsPressure() const {
		return state == NodePressureState::kPressure;
	}

	NodePressureState state = NodePressureState::kNoPressure;
};


using RoomNodePlayerStatsPtrPbComponent = std::shared_ptr<GameNodePlayerInfoPBComponent>;