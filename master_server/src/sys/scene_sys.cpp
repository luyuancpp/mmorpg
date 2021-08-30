#include "scene_sys.hpp"

#include "src/game_logic/comp/server_list.hpp"
#include "src/factories/scene_factories.hpp"

namespace master
{
void EnterScene(entt::registry& reg, const EnterSceneParam& param)
{
    auto scene_entity = param.scene_entity_;
    auto& player_entities =  reg.get<common::PlayerEntities>(scene_entity);
    player_entities.emplace(param.enter_entity_);
    reg.emplace<common::SceneEntityId>(param.enter_entity_, scene_entity);
    auto p_server_data = reg.try_get<common::GameServerDataPtr>(scene_entity);
    if (nullptr == p_server_data)
    {
        return;
    }
    (*p_server_data)->OnPlayerEnter();
}

void LeaveScene(entt::registry& reg, const LeaveSceneParam& param)
{
    auto leave_entity = param.leave_entity_;
    auto& player_scene_entity = reg.get<common::SceneEntityId>(leave_entity);
    auto scene_entity = player_scene_entity.scene_entity();
    auto& player_entities = reg.get<common::PlayerEntities>(scene_entity);
    player_entities.erase(leave_entity);
    reg.remove<common::SceneEntityId>(leave_entity);
    auto p_server_data = reg.try_get<common::GameServerDataPtr>(scene_entity);
    if (nullptr == p_server_data)
    {
        return;
    }
    (*p_server_data)->OnPlayerLeave();
}

template<typename ServerStatus, typename ServerPressure>
entt::entity GetWeightRoundRobinMainSceneT(entt::registry& reg, const GetWeightRoundRobinSceneParam& param)
{
    auto& scene_map = reg.get<common::Scenes>(scenes_entity());
    auto scene_config_id = param.scene_config_id_;
    if (!scene_map.HasSceneType(scene_config_id))
    {
        return entt::null;
    }

    entt::entity scene_entity{ entt::null };
    entt::entity server_entity{ entt::null };
    std::size_t min_player_size = UINT64_MAX;
    for (auto e : reg.view<ServerStatus, ServerPressure>())
    {
        auto& scenes = reg.get<common::Scenes>(e);
        if (!scenes.HasSceneType(scene_config_id))
        {
            continue;
        }
        auto& server_data = reg.get<common::GameServerDataPtr>(e);
        std::size_t server_player_size = (*server_data).player_size();
        if (server_player_size >= min_player_size)
        {
            continue;
        }
        server_entity = e;
        min_player_size = server_player_size;   
    }
    if (entt::null == server_entity)
    {
        return scene_entity;
    }
    auto& scenes = reg.get<common::Scenes>(server_entity);
    std::size_t scene_min_player_size = UINT64_MAX;
    for (auto& ji : scenes.scenes_config_id(scene_config_id))
    {
        std::size_t scene_player_size = reg.get<common::PlayerEntities>(ji).size();
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
    auto scene_entity = GetWeightRoundRobinMainSceneT<common::GameServerStatusNormal, common::GameNoPressure>(reg, param);
    if (entt::null != scene_entity)
    {
        return scene_entity;
    }
    return GetWeightRoundRobinMainSceneT<common::GameServerStatusNormal, common::GamePressure>(reg, param);;
}

void ServerEnterPressure(entt::registry& reg, const ServerPressureParam& param)
{
    reg.remove<common::GameNoPressure>(param.server_entity_);
    reg.emplace<common::GamePressure>(param.server_entity_);
}

void ServerEnterNoPressure(entt::registry& reg, const ServerPressureParam& param)
{
    reg.remove<common::GamePressure>(param.server_entity_);
    reg.emplace<common::GameNoPressure>(param.server_entity_);
}

void ServerCrash(entt::registry& reg, const ServerCrashParam& param)
{
    reg.remove<common::GameServerStatusNormal>(param.crash_server_entity_);
    reg.emplace<common::GameServerCrash>(param.crash_server_entity_);
}

void ReplaceCrashServer(entt::registry& reg, const ReplaceCrashServerParam& param)
{
    MoveServerScene2ServerParam move_param;
    move_param.from_server_entity_ = param.cransh_server_entity_;
    move_param.to_server_entity_ = param.replace_server_entity_;
    MoveServerScene2Server(reg, move_param);
    reg.destroy(move_param.from_server_entity_);
}

}//namespace master

