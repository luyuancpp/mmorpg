#include "servernode_system.h"

#include <ranges>

#include "src/comp/server_comp.h"
#include "src/thread_local/thread_local_storage.h"

#include "component_proto/gs_node_comp.pb.h"

using GameNodePlayerInfoPtr = std::shared_ptr<GameNodeInfo>;

//从当前服务器中找到一个对应场景人数最少的
template <typename ServerType>
entt::entity GetSceneOnMinPlayerSizeNodeT(const GetSceneParam& param, const GetSceneFilterParam& filter_state_param)
{
	auto scene_config_id = param.scene_conf_id_;
	entt::entity server{entt::null};
	std::size_t min_server_player_size = UINT64_MAX;
	for (auto entity : tls.registry.view<ServerType>())
	{
		//如果最少人数的服务器没有这个场景咋办
		//所以优先判断有没有场景
		if (const auto& server_comp = tls.registry.get<ServerComp>(entity);
			!server_comp.IsStateNormal() ||
			server_comp.GetScenesListByConfig(scene_config_id).empty() ||
				server_comp.get_server_pressure_state() != filter_state_param.node_pressure_state_)
		{
			continue;
		}
		auto server_player_size = (*tls.registry.get<GameNodePlayerInfoPtr>(entity)).player_size();
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
		const auto& scene_it : server_scenes | std::views::values)
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
entt::entity GetNotFullSceneT(const GetSceneParam& param, const GetSceneFilterParam& filter_state_param)
{
	auto scene_config_id = param.scene_conf_id_;
	entt::entity server{entt::null};
	for (auto entity : tls.registry.view<ServerType>())
	{
		if (const auto& server_comp = tls.registry.get<ServerComp>(entity);
			!server_comp.IsStateNormal() ||
			server_comp.GetScenesListByConfig(scene_config_id).empty() ||
				server_comp.get_server_pressure_state() != filter_state_param.node_pressure_state_)
		{
			continue;
		}
		auto server_player_size = (*tls.registry.get<GameNodePlayerInfoPtr>(entity)).player_size();
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
	     const auto& scene_it : server_scenes | std::views::values)
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

entt::entity ServerNodeSystem::GetSceneOnMinPlayerSizeNode(const GetSceneParam& param)
{
	constexpr GetSceneFilterParam get_scene_filter_param;
	if (const auto scene = GetSceneOnMinPlayerSizeNodeT<MainSceneServer>(param, get_scene_filter_param); entt::null != scene)
	{
		return scene;
	}
	return GetSceneOnMinPlayerSizeNodeT<MainSceneServer>(param, get_scene_filter_param);
}

entt::entity ServerNodeSystem::GetNotFullScene(const GetSceneParam& param)
{
	GetSceneFilterParam get_scene_filter_param;
	if (const auto scene_entity = GetNotFullSceneT<MainSceneServer>(param, get_scene_filter_param); entt::null != scene_entity)
	{
		return scene_entity;
	}
	get_scene_filter_param.node_pressure_state_ = NodePressureState::kPressure;
	return GetNotFullSceneT<MainSceneServer>(param, get_scene_filter_param);
}

void ServerNodeSystem::NodeEnterPressure(entt::entity node)
{
	auto* const server_comp = tls.registry.try_get<ServerComp>(node);
	if (nullptr == server_comp)
	{
		return;
	}
	server_comp->SetNodePressureState(NodePressureState::kPressure);
}

void ServerNodeSystem::NodeEnterNoPressure(entt::entity node)
{
	auto* const server_comp = tls.registry.try_get<ServerComp>(node);
	if (nullptr == server_comp)
	{
		return;
	}
	server_comp->SetNodePressureState(NodePressureState::kNoPressure);
}

void ServerNodeSystem::SetNodeState(entt::entity node, NodeState node_state)
{
	auto* const try_server_comp = tls.registry.try_get<ServerComp>(node);
	if (nullptr == try_server_comp)
	{
		return;
	}
	try_server_comp->SetNodeState(node_state);
}

