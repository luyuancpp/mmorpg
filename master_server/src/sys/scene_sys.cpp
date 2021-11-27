#include "scene_sys.hpp"

#include "src/game_logic/comp/server_list.hpp"
#include "src/factories/scene_factories.hpp"

using namespace common;

namespace master
{
void EnterScene(entt::registry& reg, const EnterSceneParam& param)
{
    auto scene_entity = param.scene_entity_;
    auto& player_entities =  reg.get<PlayerEntities>(scene_entity);
    player_entities.emplace(param.enter_entity_);
    reg.emplace<SceneEntityId>(param.enter_entity_, scene_entity);
    auto p_server_data = reg.try_get<GameServerDataPtr>(scene_entity);
    if (nullptr == p_server_data)
    {
        return;
    }
    (*p_server_data)->OnPlayerEnter();
}

void LeaveScene(entt::registry& reg, const LeaveSceneParam& param)
{
    auto leave_entity = param.leave_entity_;
    auto& player_scene_entity = reg.get<SceneEntityId>(leave_entity);
    auto scene_entity = player_scene_entity.scene_entity();
    auto& player_entities = reg.get<PlayerEntities>(scene_entity);
    player_entities.erase(leave_entity);
    reg.remove<SceneEntityId>(leave_entity);
    auto p_server_data = reg.try_get<GameServerDataPtr>(scene_entity);
    if (nullptr == p_server_data)
    {
        return;
    }
    (*p_server_data)->OnPlayerLeave();
}

template<typename ServerType,typename ServerStatus, typename ServerPressure>
entt::entity GetWeightRoundRobinMainSceneT(entt::registry& reg, const GetWeightRoundRobinSceneParam& param)
{
    auto scene_config_id = param.scene_config_id_;
    entt::entity server_entity{ entt::null };
    std::size_t min_player_size = UINT64_MAX;
    for (auto e : reg.view<ServerType, ServerStatus, ServerPressure>())
    {
        auto& scenes = reg.get<Scenes>(e);
        if (!scenes.HasSceneConfig(scene_config_id))
        {
            continue;
        }
        auto& server_data = reg.get<GameServerDataPtr>(e);
        std::size_t server_player_size = (*server_data).player_size();
        if (server_player_size >= min_player_size)
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
    auto& scenes = reg.get<Scenes>(server_entity);
    std::size_t scene_min_player_size = UINT64_MAX;
    for (auto& ji : scenes.scenes_config_id(scene_config_id))
    {
        std::size_t scene_player_size = reg.get<PlayerEntities>(ji).size();
        if (scene_player_size >= scene_min_player_size)
        {
            continue;
        }
        scene_min_player_size = scene_player_size;
        scene_entity = ji;
    }
    return scene_entity;
}

entt::entity GetWeightRoundRobinMainScene(entt::registry& reg, const GetWeightRoundRobinSceneParam& param)
{
    auto scene_entity = GetWeightRoundRobinMainSceneT<MainSceneServer, GameServerStatusNormal, GameNoPressure>(reg, param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    return GetWeightRoundRobinMainSceneT<MainSceneServer, GameServerStatusNormal, GamePressure>(reg, param);
}

entt::entity GetWeightRoundRobinRoomScene(entt::registry& reg, const GetWeightRoundRobinSceneParam& param)
{
    auto scene_entity = GetWeightRoundRobinMainSceneT<RoomSceneServer, GameServerStatusNormal, GameNoPressure>(reg, param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    return GetWeightRoundRobinMainSceneT<RoomSceneServer, GameServerStatusNormal, GamePressure>(reg, param);
}

void ServerEnterPressure(entt::registry& reg, const ServerPressureParam& param)
{
    reg.remove<GameNoPressure>(param.server_entity_);
    reg.emplace<GamePressure>(param.server_entity_);
}

void ServerEnterNoPressure(entt::registry& reg, const ServerPressureParam& param)
{
    reg.remove<GamePressure>(param.server_entity_);
    reg.emplace<GameNoPressure>(param.server_entity_);
}

void ServerCrashed(entt::registry& reg, const ServerCrashParam& param)
{
    reg.remove<GameServerStatusNormal>(param.crash_server_entity_);
    reg.emplace<GameServerCrash>(param.crash_server_entity_);
}



void ServerMaintain(entt::registry& reg, const MaintainServerParam& param)
{
    reg.remove<GameServerStatusNormal>(param.maintain_server_entity_);
    reg.emplace<GameServerMainTain>(param.maintain_server_entity_);
}
}//namespace master

