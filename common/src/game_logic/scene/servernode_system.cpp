#include "servernode_system.h"

#include "src/game_logic/comp/server_comp.h"
#include "src/game_logic/thread_local/thread_local_storage.h"

#include "component_proto/gs_node_comp.pb.h"

using GsNodePlayerInfoPtr = std::shared_ptr<GsNodePlayerInfo>;

//从当前服务器中找到一个对应场景人数最少的
template <typename ServerType>
entt::entity GetMinPlayerSizeServerScene(const GetSceneParam& param, const GetSceneFilterParam& filter_state_param)
{
	auto scene_config_id = param.scene_confid_;
	entt::entity server{entt::null};
	std::size_t min_server_player_size = UINT64_MAX;
	for (auto entity : tls.registry.view<ServerType>())
	{
		//如果最少人数的服务器没有这个场景咋办
		//所以优先判断有没有场景
		if (const auto& try_server_comp = tls.registry.get<ServerComp>(entity);
			!try_server_comp.IsStateNormal() ||
			!try_server_comp.HasConfig(scene_config_id ||
				try_server_comp.get_server_pressure_state() != filter_state_param.server_pressure_state_))
		{
			continue;
		}
		auto server_player_size = (*tls.registry.get<GsNodePlayerInfoPtr>(entity)).player_size();
		if (server_player_size >= min_server_player_size || server_player_size >= kMaxServerPlayerSize)
		{
			continue;
		}
		server = entity;
		min_server_player_size = server_player_size;
	}

	if (entt::null == server)
	{
		return entt::null;
	}

	entt::entity scene{entt::null};
	const auto& server_comps = tls.registry.get<ServerComp>(server);
	std::size_t min_scene_player_size = UINT64_MAX;
	for (const auto& server_scenes = server_comps.GetScenesListByConfig(scene_config_id);
		const auto& scene_it : server_scenes)
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

//选择不满人的服务器场景
template <typename ServerType>
entt::entity GetSceneServerNotFull(const GetSceneParam& param, const GetSceneFilterParam& filter_state_param)
{
	auto scene_config_id = param.scene_confid_;
	entt::entity server{entt::null};
	for (auto entity : tls.registry.view<ServerType>())
	{
		if (const auto& try_server_comp = tls.registry.get<ServerComp>(entity);
			!try_server_comp.IsStateNormal() ||
			!try_server_comp.HasConfig(scene_config_id ||
				try_server_comp.get_server_pressure_state() != filter_state_param.server_pressure_state_))
		{
			continue;
		}
		auto server_player_size = (*tls.registry.get<GsNodePlayerInfoPtr>(entity)).player_size();
		if (server_player_size >= kMaxServerPlayerSize)
		{
			continue;
		}
		server = entity;
		break;
	}

	if (entt::null == server)
	{
		return entt::null;
	}

	entt::entity scene{entt::null};
	const auto& server_comps = tls.registry.get<ServerComp>(server);
	for (const auto& server_scenes = server_comps.GetScenesListByConfig(scene_config_id);
	     const auto& scene_it : server_scenes)
	{
		if (const auto scene_player_size = tls.registry.get<ScenePlayers>(scene_it).size();
			scene_player_size >= kMaxScenePlayerSize)
		{
			continue;
		}
		scene = scene_it;
		break;
	}
	return scene;
}

entt::entity ServerNodeSystem::GetWeightRoundRobinMainScene(const GetSceneParam& param)
{
	constexpr GetSceneFilterParam get_scene_filter_param;
	if (const auto scene = GetMinPlayerSizeServerScene<MainSceneServer>(param, get_scene_filter_param); entt::null != scene)
	{
		return scene;
	}
	return GetMinPlayerSizeServerScene<MainSceneServer>(param, get_scene_filter_param);
}

entt::entity ServerNodeSystem::GetMainSceneNotFull(const GetSceneParam& param)
{
	GetSceneFilterParam get_scene_filter_param;
	if (const auto scene_entity = GetSceneServerNotFull<MainSceneServer>(param, get_scene_filter_param); entt::null != scene_entity)
	{
		return scene_entity;
	}
	get_scene_filter_param.server_pressure_state_ = ServerPressureState::kPressure;
	return GetSceneServerNotFull<MainSceneServer>(param, get_scene_filter_param);
}

void ServerNodeSystem::ServerEnterPressure(const ServerPressureParam& param)
{
	auto* const try_server_comp = tls.registry.try_get<ServerComp>(param.server_);
	if (nullptr == try_server_comp)
	{
		return;
	}
	try_server_comp->SetServerPressureState(ServerPressureState::kPressure);
}

void ServerNodeSystem::ServerEnterNoPressure(const ServerPressureParam& param)
{
	auto* const try_server_comp = tls.registry.try_get<ServerComp>(param.server_);
	if (nullptr == try_server_comp)
	{
		return;
	}
	try_server_comp->SetServerPressureState(ServerPressureState::kNoPressure);
}

void ServerNodeSystem::set_server_state(const ServerStateParam& param)
{
	auto* const try_server_comp = tls.registry.try_get<ServerComp>(param.node_entity_);
	if (nullptr == try_server_comp)
	{
		return;
	}
	try_server_comp->SetServerState(param.server_state_);
}

