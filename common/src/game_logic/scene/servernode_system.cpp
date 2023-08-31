#include "servernode_system.h"

#include "src/game_logic/comp/server_comp.h"
#include "src/game_logic/thread_local/thread_local_storage.h"

#include "component_proto/gs_node_comp.pb.h"

using GsNodePlayerInfoPtr = std::shared_ptr<GsNodePlayerInfo>;

//从当前符服务器中找到一个对应场景人数最少的
template<typename ServerType>
entt::entity GetWeightRoundRobinSceneT(const GetSceneParam& param, const GetSceneFilterParam& filter_state_param)
{
    auto scene_config_id = param.scene_confid_;
    entt::entity server{ entt::null };
    std::size_t min_server_player_size = UINT64_MAX;
    for (auto e : tls.registry.view<ServerType>())
    {
        //如果最少人数的服务器没有这个场景咋办
        //所以优先判断有没有场景
		const auto& try_server_comp = tls.registry.get<ServerComp>(e);
		if (!try_server_comp.IsStateNormal() ||
			!try_server_comp.HasConfig(scene_config_id || 
            try_server_comp.get_server_pressure_state() != filter_state_param.server_pressure_state_))
		{
			continue;
		}
        std::size_t server_player_size = (*tls.registry.get<GsNodePlayerInfoPtr>(e)).player_size();
        if (server_player_size >= min_server_player_size || server_player_size >= kMaxServerPlayerSize)
        {
            continue;
        }
        server = e;
        min_server_player_size = server_player_size;   
    }
    entt::entity scene{ entt::null };
    if (entt::null == server)
    {
        return scene;
    }
    auto& scenes = tls.registry.get<ServerComp>(server);
    std::size_t min_scene_player_size = UINT64_MAX;
    const auto& server_scenes = scenes.GetScenesListByConfig(scene_config_id);
    for (auto& ji : server_scenes)
    {
        std::size_t scene_player_size = tls.registry.get<ScenePlayers>(ji).size();
        if (scene_player_size >= min_scene_player_size || scene_player_size >= kMaxScenePlayerSize)
        {
            continue;
        }
        min_scene_player_size = scene_player_size;
        scene = ji;
    }
    return scene;
}

//选择不满人的服务器场景
template<typename ServerType>
entt::entity GetMainSceneNotFullT(const GetSceneParam& param, const GetSceneFilterParam& filter_state_param)
{
	auto scene_config_id = param.scene_confid_;
	entt::entity server{ entt::null };
	for (auto e : tls.registry.view<ServerType>())
	{
		const auto& try_server_comp = tls.registry.get<ServerComp>(e);
		if (!try_server_comp.IsStateNormal() ||
			!try_server_comp.HasConfig(scene_config_id ||
			try_server_comp.get_server_pressure_state() != filter_state_param.server_pressure_state_))
		{
			continue;
		}
		std::size_t server_player_size = (*tls.registry.get<GsNodePlayerInfoPtr>(e)).player_size();
		if (server_player_size >= kMaxServerPlayerSize)
		{
			continue;
		}
		server = e;
        break;
	}
	entt::entity scene{ entt::null };
	if (entt::null == server)
	{
		return scene;
	}
	auto& scenes = tls.registry.get<ServerComp>(server);
	auto& server_scenes = scenes.GetScenesListByConfig(scene_config_id);
	for (auto& ji : server_scenes)
	{
		std::size_t scene_player_size = tls.registry.get<ScenePlayers>(ji).size();
		if (scene_player_size >= kMaxScenePlayerSize)
		{
			continue;
		}
		scene = ji;
        break;
	}
	return scene;
}

entt::entity ServerNodeSystem::GetWeightRoundRobinMainScene(const GetSceneParam& param)
{
    GetSceneFilterParam get_scene_filter_param;
    auto scene = GetWeightRoundRobinSceneT<MainSceneServer>( param, get_scene_filter_param);
    if (entt::null != scene)
    {
        return scene;
    }
    return GetWeightRoundRobinSceneT<MainSceneServer>( param, get_scene_filter_param);
}

entt::entity ServerNodeSystem::GetWeightRoundRobinRoomScene(const GetSceneParam& param)
{
    GetSceneFilterParam get_scene_filter_param;
    auto scene_entity = GetWeightRoundRobinSceneT<RoomSceneServer>( param, get_scene_filter_param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    get_scene_filter_param.server_pressure_state_ = ServerPressureState::kPressure;
    return GetWeightRoundRobinSceneT<RoomSceneServer>(param, get_scene_filter_param);
}

entt::entity ServerNodeSystem::GetMainSceneNotFull(const GetSceneParam& param)
{
    GetSceneFilterParam get_scene_filter_param;
	auto scene_entity = GetMainSceneNotFullT<MainSceneServer>(param, get_scene_filter_param);
	if (entt::null != scene_entity)
	{
		return scene_entity;
	}
    get_scene_filter_param.server_pressure_state_ = ServerPressureState::kPressure;
	return GetMainSceneNotFullT<MainSceneServer>(param, get_scene_filter_param);
}

void ServerNodeSystem::ServerEnterPressure(const ServerPressureParam& param)
{
	auto try_server_copm = tls.registry.try_get<ServerComp>(param.server_);
	if (nullptr == try_server_copm)
	{
		return;
	}
	try_server_copm->SetServerPressureState(ServerPressureState::kPressure);
}

void ServerNodeSystem::ServerEnterNoPressure( const ServerPressureParam& param)
{
	auto try_server_copm = tls.registry.try_get<ServerComp>(param.server_);
	if (nullptr == try_server_copm)
	{
		return;
	}
	try_server_copm->SetServerPressureState(ServerPressureState::kNoPressure);
}

void ServerNodeSystem::set_server_state( const ServerStateParam& param)
{
    auto try_server_copm =  tls.registry.try_get<ServerComp>(param.node_entity_);
    if (nullptr == try_server_copm)
    {
        return;
    }
    try_server_copm->SetServerState(param.server_state_);
}

