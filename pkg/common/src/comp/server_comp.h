#pragma once

#include <ranges>

#include "src/type_define/type_define.h"
#include "src/constants/server_constants.h"
#include "src/thread_local/thread_local_storage_common_logic.h"
#include "src/thread_local/thread_local_storage.h"
#include "component_proto/scene_comp.pb.h"

using ConfigSceneListType = std::unordered_map<uint32_t, SceneList>;
using ScenePlayers = EntitySet; //弱引用，要解除玩家和场景的耦合

entt::entity global_entity();

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
	[[nodiscard]] const ConfigSceneListType& GetScenesList() const
	{
		return conf_id_scene_list_;
	}

	inline std::size_t GetSceneSize() const
	{
		std::size_t size = 0;
		for (const auto& val : conf_id_scene_list_ | std::views::values)
		{
			size += val.size();
		}
		return size;
	}

	[[nodiscard]] const SceneList& GetScenesListByConfig(uint32_t scene_config_id) const
	{
		const auto list_const_iterator = conf_id_scene_list_.find(scene_config_id);
		if (list_const_iterator == conf_id_scene_list_.end())
		{
			static const SceneList empty_result;
			return empty_result;
		}
		return list_const_iterator->second;
	}

	[[nodiscard]] static entt::entity GetScenesListByGuid(const Guid guid)
	{
		const auto scene_it = cl_tls.scene_list().find(guid);
		if (scene_it == cl_tls.scene_list().end())
		{
			return entt::null;
		}
		return scene_it->second;
	}

	void AddScene(entt::entity scene)
	{
		const auto& scene_info = tls.registry.get<SceneInfo>(scene);
		cl_tls.scene_list().emplace(scene_info.guid(), scene);
		conf_id_scene_list_[scene_info.scene_confid()].emplace(scene_info.guid(), scene);
	}

	inline void RemoveScene(const entt::entity scene)
	{
		const auto& scene_info = tls.registry.get<SceneInfo>(scene);
		cl_tls.scene_list().erase(scene_info.guid());
		conf_id_scene_list_[scene_info.scene_confid()].erase(scene_info.guid());
		tls.registry.destroy(scene);
	}

	[[nodiscard]] entt::entity GetMinPlayerSizeSceneByConfigId(const uint32_t scene_config_id) const
	{
		const auto& scene_list = GetScenesListByConfig(scene_config_id);
		if (scene_list.empty())
		{
			return entt::null;
		}
		entt::entity scene{entt::null};
		std::size_t min_scene_player_size = UINT64_MAX;
		for (const auto& scene_it : scene_list | std::views::values)
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

	inline void SetNodeState(const NodeState state) { node_state_ = state; }
	[[nodiscard]] NodeState GetNodeState() const { return node_state_; }
	inline bool IsStateNormal() const { return node_state_ == NodeState::kNormal; }

	inline void SetNodePressureState(const NodePressureState state) { node_pressure_state_ = state; }
	[[nodiscard]] NodePressureState get_server_pressure_state() const { return node_pressure_state_; }
	inline bool IsNodeNoPressure() const { return node_pressure_state_ == NodePressureState::kNoPressure; }
	inline bool IsNodePressure() const { return node_pressure_state_ == NodePressureState::kPressure; }

	[[nodiscard]] inline  ServerSceneType GetServerSceneType() const { return node_scene_type_; }

	inline void SetNodeSceneType(const ServerSceneType server_scene_type) { node_scene_type_ = server_scene_type; }

private:
	ConfigSceneListType conf_id_scene_list_; //配置表对应的场景列表,不要对它进行任何操作了,只是用来优化性能用
	NodeState node_state_{NodeState::kNormal};
	NodePressureState node_pressure_state_{NodePressureState::kNoPressure};
	ServerSceneType node_scene_type_{ServerSceneType::kMainSceneServer};
};
