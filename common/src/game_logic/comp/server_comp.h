#pragma once

#include "src/common_type/common_type.h"
#include "src/game_logic/constants/server_constants.h"
#include "src/game_logic/thread_local/thread_local_storage.h"

#include "src/pb/pbc/component_proto/scene_comp.pb.h"

using SceneList = std::unordered_map<Guid, entt::entity>;
using Uint32KeyEntitySetValue = std::unordered_map<uint32_t, EntitySet>;
using ScenePlayers = EntitySet; //弱引用，要解除玩家和场景的耦合

struct MainSceneServer
{
};

struct CrossMainSceneServer
{
};

struct RoomSceneServer
{
};

struct CrossRoomSceneServer
{
};

class ServerComp
{
public:
	[[nodiscard]] const SceneList& GetScenesList() const { return scene_list_; }
	inline std::size_t GetSceneSize() const { return scene_list_.size(); }
	inline bool IsSceneEmpty() const { return scene_list_.empty(); }

	[[nodiscard]] const EntitySet& GetScenesListByConfig(uint32_t scene_config_id) const
	{
		const auto list_const_iterator = conf_id_scene_list_.find(scene_config_id);
		if (list_const_iterator == conf_id_scene_list_.end())
		{
			static const EntitySet empty_result;
			return empty_result;
		}
		return list_const_iterator->second;
	}

	[[nodiscard]] entt::entity GetScenesListByGuid(Guid guid) const
	{
		const auto scene_it = scene_list_.find(guid);
		if (scene_it == scene_list_.end())
		{
			return entt::null;
		}
		return scene_it->second;
	}

	void AddScene(entt::entity scene)
	{
		const auto& scene_info = tls.registry.get<SceneInfo>(scene);
		scene_list_.emplace(scene_info.scene_id(), scene);
		conf_id_scene_list_[scene_info.scene_confid()].emplace(scene);
	}

	inline void RemoveScene(const entt::entity scene)
	{
		const auto& scene_info = tls.registry.get<SceneInfo>(scene);
		scene_list_.erase(scene_info.scene_id());
		const auto scene_it = conf_id_scene_list_.find(scene_info.scene_confid());
		if (scene_it == conf_id_scene_list_.end())
		{
			return;
		}
		scene_it->second.erase(scene);
	}

	[[nodiscard]] entt::entity GetMinPlayerSizeSceneByConfigId(uint32_t scene_config_id) const;

	inline void SetNodeState(const NodeState state) { node_state_ = state; }
	[[nodiscard]] NodeState GetNodeState() const { return node_state_; }
	inline bool IsStateNormal() const { return node_state_ == NodeState::kNormal; }

	inline void SetNodePressureState(const NodePressureState state) { node_pressure_state_ = state; }
	[[nodiscard]] NodePressureState get_server_pressure_state() const { return node_pressure_state_; }
	inline bool IsNodeNoPressure() const { return node_pressure_state_ == NodePressureState::kNoPressure; }
	inline bool IsNodePressure() const { return node_pressure_state_ == NodePressureState::kPressure; }

	inline [[nodiscard]] ServerSceneType GetServerSceneType() const { return node_scene_type_; }

	inline void SetNodeSceneType(const ServerSceneType server_scene_type) { node_scene_type_ = server_scene_type; }

private:
	SceneList scene_list_;
	Uint32KeyEntitySetValue conf_id_scene_list_; //配置表对应的场景列表
	NodeState node_state_{NodeState::kNormal};
	NodePressureState node_pressure_state_{NodePressureState::kNoPressure};
	ServerSceneType node_scene_type_{ServerSceneType::kMainSceneServer};
};


[[nodiscard]] inline entt::entity ServerComp::GetMinPlayerSizeSceneByConfigId(uint32_t scene_conf_id) const
{
	const auto& scene_list = GetScenesListByConfig(scene_conf_id);
	if (scene_list.empty())
	{
		return entt::null;
	}
	entt::entity scene{entt::null};
	std::size_t min_scene_player_size = UINT64_MAX;
	for (const auto& scene_it : scene_list)
	{
		const auto scene_player_size = tls.registry.get<ScenePlayers>(scene_it).size();
		if (scene_player_size >= min_scene_player_size || scene_player_size >= kMaxScenePlayerSize)
		{
			continue;
		}
		min_scene_player_size = scene_player_size;
		scene = scene_it;
	}
	return scene;
}
