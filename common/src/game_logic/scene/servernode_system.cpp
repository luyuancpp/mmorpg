#include "servernode_system.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/thread_local/game_registry.h"

//�ӵ�ǰ�����������ҵ�һ����Ӧ�����������ٵ�
template<typename ServerType,typename ServerStatus, typename ServerPressure>
entt::entity GetWeightRoundRobinSceneT(const GetSceneParam& param)
{
    //todo������������ķ�����û���������զ��
    auto scene_confid = param.scene_confid_;
    entt::entity server{ entt::null };
    std::size_t min_server_player_size = UINT64_MAX;
    for (auto e : registry.view<ServerType, ServerStatus, ServerPressure>())
    {
        if (!registry.get<ConfigSceneMap>(e).HasConfig(scene_confid))//�����ж���û�г���
        {
            continue;
        }
        std::size_t server_player_size = (*registry.get<GsNodePlayerInfoPtr>(e)).player_size();
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
    auto& scenes = registry.get<ConfigSceneMap>(server);
    std::size_t min_scene_player_size = UINT64_MAX;
    auto& server_scenes = scenes.confid_sceneslist(scene_confid);
    for (auto& ji : server_scenes)
    {
        std::size_t scene_player_size = registry.get<ScenePlayers>(ji).size();
        if (scene_player_size >= min_scene_player_size || scene_player_size >= kMaxScenePlayerSize)
        {
            continue;
        }
        min_scene_player_size = scene_player_size;
        scene = ji;
    }
    return scene;
}

//ѡ�����˵÷���������
template<typename ServerType, typename ServerStatus, typename ServerPressure>
entt::entity GetMainSceneNotFullT(const GetSceneParam& param)
{
	auto scene_config_id = param.scene_confid_;
	entt::entity server{ entt::null };
	for (auto e : registry.view<ServerType, ServerStatus, ServerPressure>())
	{
		if (!registry.get<ConfigSceneMap>(e).HasConfig(scene_config_id))
		{
			continue;
		}
		std::size_t server_player_size = (*registry.get<GsNodePlayerInfoPtr>(e)).player_size();
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
	auto& scenes = registry.get<ConfigSceneMap>(server);
	auto& server_scenes = scenes.confid_sceneslist(scene_config_id);
	for (auto& ji : server_scenes)
	{
		std::size_t scene_player_size = registry.get<ScenePlayers>(ji).size();
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
    auto scene = GetWeightRoundRobinSceneT<MainSceneServer, GSNormal, NoPressure>( param);
    if (entt::null != scene)
    {
        return scene;
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
	auto scene_entity = GetMainSceneNotFullT<MainSceneServer, GSNormal, NoPressure>(param);
	if (entt::null != scene_entity)
	{
		return scene_entity;
	}
	return GetMainSceneNotFullT<MainSceneServer, GSNormal, Pressure>(param);
}

void ServerNodeSystem::ServerEnterPressure(const ServerPressureParam& param)
{
    registry.remove<NoPressure>(param.server_);
    registry.emplace<Pressure>(param.server_);
}

void ServerNodeSystem::ServerEnterNoPressure( const ServerPressureParam& param)
{
    registry.remove<Pressure>(param.server_);
    registry.emplace<NoPressure>(param.server_);
}

void ServerNodeSystem::ServerCrashed( const ServerCrashParam& param)
{
    registry.remove<GSNormal>(param.crash_server_);
    registry.emplace<GSCrash>(param.crash_server_);
}

void ServerNodeSystem::ServerMaintain(const MaintainServerParam& param)
{
    registry.remove<GSNormal>(param.maintain_server_);
    registry.emplace<GSMainTain>(param.maintain_server_);
}

