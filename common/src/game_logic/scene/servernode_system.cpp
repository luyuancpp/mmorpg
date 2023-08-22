#include "servernode_system.h"

#include "src/game_logic/comp/server_comp.h"
#include "src/game_logic/thread_local/thread_local_storage.h"

//从当前符服务器中找到一个对应场景人数最少的
template<typename ServerType,typename ServerPressure>
entt::entity GetWeightRoundRobinSceneT(const GetSceneParam& param)
{
    auto scene_config_id = param.scene_confid_;
    entt::entity server{ entt::null };
    std::size_t min_server_player_size = UINT64_MAX;
    for (auto e : tls.registry.view<ServerType,  ServerPressure>())
    {
        //如果最少人数的服务器没有这个场景咋办
        //所以优先判断有没有场景
		const auto& try_server_comp = tls.registry.get<ServerComp>(e);
		if (!try_server_comp.is_state_normal() ||
			!try_server_comp.HasConfig(scene_config_id))
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
    const auto& server_scenes = scenes.get_sceneslist_by_config(scene_config_id);
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
template<typename ServerType, typename ServerPressure>
entt::entity GetMainSceneNotFullT(const GetSceneParam& param)
{
	auto scene_config_id = param.scene_confid_;
	entt::entity server{ entt::null };
	for (auto e : tls.registry.view<ServerType, ServerPressure>())
	{
        auto& try_server_comp = tls.registry.get<ServerComp>(e);
		if (!try_server_comp.is_state_normal() ||
            !try_server_comp.HasConfig(scene_config_id))
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
	auto& server_scenes = scenes.get_sceneslist_by_config(scene_config_id);
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
    auto scene = GetWeightRoundRobinSceneT<MainSceneServer, NoPressure>( param);
    if (entt::null != scene)
    {
        return scene;
    }
    return GetWeightRoundRobinSceneT<MainSceneServer,  Pressure>( param);
}

entt::entity ServerNodeSystem::GetWeightRoundRobinRoomScene(const GetSceneParam& param)
{
    auto scene_entity = GetWeightRoundRobinSceneT<RoomSceneServer, NoPressure>( param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    return GetWeightRoundRobinSceneT<RoomSceneServer, Pressure>(param);
}

entt::entity ServerNodeSystem::GetMainSceneNotFull(const GetSceneParam& param)
{
	auto scene_entity = GetMainSceneNotFullT<MainSceneServer, NoPressure>(param);
	if (entt::null != scene_entity)
	{
		return scene_entity;
	}
	return GetMainSceneNotFullT<MainSceneServer,  Pressure>(param);
}

void ServerNodeSystem::ServerEnterPressure(const ServerPressureParam& param)
{
    tls.registry.remove<NoPressure>(param.server_);
    tls.registry.emplace<Pressure>(param.server_);
}

void ServerNodeSystem::ServerEnterNoPressure( const ServerPressureParam& param)
{
    tls.registry.remove<Pressure>(param.server_);
    tls.registry.emplace<NoPressure>(param.server_);
}

void ServerNodeSystem::set_server_state( const ServerStateParam& param)
{
    auto try_server_copm =  tls.registry.try_get<ServerComp>(param.node_entity_);
    if (nullptr == try_server_copm)
    {
        return;
    }
    try_server_copm->set_sever_state(param.server_state_);
}

