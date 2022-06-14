#include "servernode_system.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/scene/scene_factories.h"

//从当前符服务器中找到一个对应场景人数最少的
template<typename ServerType,typename ServerStatus, typename ServerPressure>
entt::entity GetWeightRoundRobinSceneT(const GetSceneParam& param)
{
    //如果最少人数的服务器没有这个场景咋办
    auto scene_confid = param.scene_confid_;
    entt::entity server_entity{ entt::null };
    std::size_t min_player_size = UINT64_MAX;
    for (auto e : registry.view<ServerType, ServerStatus, ServerPressure>())
    {
        if (!registry.get<ConfigSceneMap>(e).HasConfig(scene_confid))
        {
            continue;
        }
        std::size_t server_player_size = (*registry.get<GsDataPtr>(e)).player_size();
        if (server_player_size >= min_player_size || server_player_size >= kMaxServerPlayerSize)
        {
            continue;
        }
        server_entity = e;
        min_player_size = server_player_size;   
    }
    entt::entity scene_entity{ entt::null };
    if (entt::null == server_entity)
    {
        return scene_entity;
    }
    auto& scenes = registry.get<ConfigSceneMap>(server_entity);
    std::size_t scene_min_player_size = UINT64_MAX;
    auto& server_scenes = scenes.confid_sceneslist(scene_confid);
    for (auto& ji : server_scenes)
    {
        std::size_t scene_player_size = registry.get<ScenePlayers>(ji).size();
        if (scene_player_size >= scene_min_player_size || scene_player_size >= kMaxScenePlayerSize)
        {
            continue;
        }
        scene_min_player_size = scene_player_size;
        scene_entity = ji;
    }
    return scene_entity;
}

//选择不满人得服务器场景
template<typename ServerType, typename ServerStatus, typename ServerPressure>
entt::entity GetGetMainSceneNotFullT(const GetSceneParam& param)
{
	auto scene_config_id = param.scene_confid_;
	entt::entity server_entity{ entt::null };
	for (auto e : registry.view<ServerType, ServerStatus, ServerPressure>())
	{
		if (!registry.get<ConfigSceneMap>(e).HasConfig(scene_config_id))
		{
			continue;
		}
		std::size_t server_player_size = (*registry.get<GsDataPtr>(e)).player_size();
		if (server_player_size >= kMaxServerPlayerSize)
		{
			continue;
		}
		server_entity = e;
        break;
	}
	entt::entity scene_entity{ entt::null };
	if (entt::null == server_entity)
	{
		return scene_entity;
	}
	auto& scenes = registry.get<ConfigSceneMap>(server_entity);
	auto& server_scenes = scenes.confid_sceneslist(scene_config_id);
	for (auto& ji : server_scenes)
	{
		std::size_t scene_player_size = registry.get<ScenePlayers>(ji).size();
		if (scene_player_size >= kMaxScenePlayerSize)
		{
			continue;
		}
		scene_entity = ji;
        break;
	}
	return scene_entity;
}

entt::entity ServerNodeSystem::GetWeightRoundRobinMainScene(const GetSceneParam& param)
{
    auto scene_entity = GetWeightRoundRobinSceneT<MainSceneServer, GSNormal, NoPressure>( param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    return GetWeightRoundRobinSceneT<MainSceneServer, GSNormal, Pressure>( param);
}

entt::entity ServerNodeSystem::GetWeightRoundRobinRoomScene(const GetSceneParam& param)
{
    auto scene_entity = GetWeightRoundRobinSceneT<RoomSceneServer, GSNormal, NoPressure>( param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    return GetWeightRoundRobinSceneT<RoomSceneServer, GSNormal, Pressure>(param);
}

entt::entity ServerNodeSystem::GetMainSceneNotFull(const GetSceneParam& param)
{
	auto scene_entity = GetGetMainSceneNotFullT<MainSceneServer, GSNormal, NoPressure>(param);
	if (entt::null != scene_entity)
	{
		return scene_entity;
	}
	return GetGetMainSceneNotFullT<MainSceneServer, GSNormal, Pressure>(param);
}


void ServerNodeSystem::ServerEnterPressure(entt::registry& reg, const ServerPressureParam& param)
{
    registry.remove<NoPressure>(param.server_);
    registry.emplace<Pressure>(param.server_);
}

void ServerNodeSystem::ServerEnterNoPressure(entt::registry& reg, const ServerPressureParam& param)
{
    registry.remove<Pressure>(param.server_);
    registry.emplace<NoPressure>(param.server_);
}

void ServerNodeSystem::ServerCrashed(entt::registry& reg, const ServerCrashParam& param)
{
    registry.remove<GSNormal>(param.crash_entity_);
    registry.emplace<GSCrash>(param.crash_entity_);
}

void ServerNodeSystem::ServerMaintain(entt::registry& reg, const MaintainServerParam& param)
{
    registry.remove<GSNormal>(param.maintain_entity_);
    registry.emplace<GSMainTain>(param.maintain_entity_);
}

