#pragma once

#include <memory>
#include <ranges>

#include "src/common_type/common_type.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/enum/server_enum.h"

#include "component_proto/gs_node_comp.pb.h"

using SceneList = std::unordered_map<Guid, entt::entity>;
using Uint32KeyEntitySetValue = std::unordered_map<uint32_t, EntitySet>;
using ScenePlayers = EntitySet;//弱引用，要解除玩家和场景的耦合

struct MainSceneServer {};
struct CrossMainSceneServer {};
struct RoomSceneServer {};
struct CrossRoomSceneServer {};


struct NoPressure {};//
struct Pressure {};//

using GsNodePlayerInfoPtr = std::shared_ptr<GsNodePlayerInfo>;

class ServerComp 
{
public:
	const Uint32KeyEntitySetValue& confid_sceneslist() const { return conf_id_scene_list_; }
	const EntitySet& get_sceneslist_by_config(uint32_t scene_config_id) const
	{
		auto it = conf_id_scene_list_.find(scene_config_id);
		if (it == conf_id_scene_list_.end())
		{
			static EntitySet s;
			return s;
		}
		return it->second;
	}

	entt::entity get_firstscene_by_configid(uint32_t scene_config_id)const
	{
		auto it = conf_id_scene_list_.find(scene_config_id);
		if (it == conf_id_scene_list_.end())
		{
			return entt::null;
		}
		if (it->second.empty())
		{
			return entt::null;
		}
		return *it->second.begin();
	}

	inline void SetServerState(ServerState state) { server_state_ = state; }
	[[nodiscard]] ServerState GetServerState() const { return server_state_; }
	inline bool IsStateNormal() const { return server_state_ == ServerState::kNormal; }

	inline void SetServerPressureState(const ServerPressureState state) { server_pressure_state_ = state; }
	[[nodiscard]] ServerPressureState get_server_pressure_state() const { return server_pressure_state_; }
	inline bool IsServerNoPressure() const { return server_pressure_state_ == ServerPressureState::kNoPressure; }
	inline bool IsServerPressure() const { return server_pressure_state_ == ServerPressureState::kPressure; }

	[[nodiscard]] ServerSceneType GetServerSceneType() const
	{
		return server_scene_type_;
	}

	void SetServerSceneType(const ServerSceneType server_scene_type)
	{
		server_scene_type_ = server_scene_type;
	}

	inline std::size_t scenes_size() const
	{
		std::size_t scene_size = 0;
		for (const auto& val : conf_id_scene_list_ | std::views::values)
		{
			scene_size += val.size();
		}
		return scene_size;
	}

	inline bool IsSceneEmpty() const
	{
		return scenes_size() == 0;
	}

	inline bool HasConfig(uint32_t scene_config_id) const { return conf_id_scene_list_.find(scene_config_id) != conf_id_scene_list_.end(); }

	void AddScene(uint32_t scene_config_id, entt::entity scene_entity)
	{
		conf_id_scene_list_[scene_config_id].emplace(scene_entity);
	}

	void RemoveScene(uint32_t scene_config_id, entt::entity scene_entity)
	{
		conf_id_scene_list_[scene_config_id].erase(scene_entity);
	}

private:
	Uint32KeyEntitySetValue conf_id_scene_list_; //配置表对应的场景列表
	ServerState server_state_{ServerState::kNormal};
	ServerPressureState server_pressure_state_{ServerPressureState::kNoPressure};
	ServerSceneType server_scene_type_{ServerSceneType::kMainSceneServer};
};
