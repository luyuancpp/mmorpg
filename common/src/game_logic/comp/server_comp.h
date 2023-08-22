#pragma once

#include <memory>

#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/common_type/common_type.h"

#include "component_proto/gs_node_comp.pb.h"

using SceneList = std::unordered_map<Guid, entt::entity>;
using Uint32KeyEntitySetValue = std::unordered_map<uint32_t, EntitySet>;
using ScenePlayers = EntitySet;//弱引用，要解除玩家和场景的耦合

struct MainSceneServer {};
struct CrossMainSceneServer {};
struct RoomSceneServer {};
struct CrossRoomSceneServer {};

struct GSNormal {};//game server 正常状态
struct GSMainTain {};//game server 维护状态
struct GSCrash {};//崩溃状态

struct NoPressure {};//
struct Pressure {};//

using GsNodePlayerInfoPtr = std::shared_ptr<GsNodePlayerInfo>;

class ServerComp 
{
public:
	const Uint32KeyEntitySetValue& confid_sceneslist() const { return confid_scenelist_; }
	const EntitySet& get_sceneslist_by_config(uint32_t scene_config_id) const
	{
		auto it = confid_scenelist_.find(scene_config_id);
		if (it == confid_scenelist_.end())
		{
			static EntitySet s;
			return s;
		}
		return it->second;
	}

	entt::entity get_firstscene_by_configid(uint32_t scene_config_id)const
	{
		auto it = confid_scenelist_.find(scene_config_id);
		if (it == confid_scenelist_.end())
		{
			return entt::null;
		}
		if (it->second.empty())
		{
			return entt::null;
		}
		return *it->second.begin();
	}

	inline std::size_t scenes_size() const {
		std::size_t s = 0;
		for (auto& it : confid_scenelist_)
		{
			s += it.second.size();
		}
		return s;
	}

	inline bool scenes_empty() const
	{
		return scenes_size() == 0;
	}

	inline bool HasConfig(uint32_t scene_config_id)const { return confid_scenelist_.find(scene_config_id) != confid_scenelist_.end(); }

	void AddScene(uint32_t scene_config_id, entt::entity scene_entity)
	{
		confid_scenelist_[scene_config_id].emplace(scene_entity);
	}

	void RemoveScene(uint32_t scene_config_id, entt::entity scene_entity)
	{
		confid_scenelist_[scene_config_id].erase(scene_entity);
	}
private:
	Uint32KeyEntitySetValue confid_scenelist_;
};