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
    auto scene_config_id = param.scene_config_id_;
    entt::entity server_entity{ entt::null };
    std::size_t min_player_size = UINT64_MAX;
    for (auto e : reg.view<ServerStatus, ServerPressure, common::MainSceneServer>())
    {
        auto& scenes = reg.get<common::Scenes>(e);
        if (!scenes.HasSceneConfig(scene_config_id))
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
    entt::entity scene_entity{ entt::null };
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

void ServerCrashed(entt::registry& reg, const ServerCrashParam& param)
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

void ServerMaintain(entt::registry& reg, const MaintainServerParam& param)
{
    reg.remove<common::GameServerStatusNormal>(param.maintain_server_entity_);
    reg.emplace<common::GameServerMainTain>(param.maintain_server_entity_);
}

void CompelChangeScene(entt::registry& reg, const CompelChangeSceneParam& param)
{
    auto new_server_entity = param.new_server_entity_;
    auto compel_entity = param.compel_change_entity_;
    auto& new_server_scene = reg.get<common::Scenes>(new_server_entity);
    auto scene_config_id = param.scene_config_id_;

    entt::entity server_scene_enitity = entt::null;

    if (!new_server_scene.HasSceneConfig(param.scene_config_id_))
    {
        MakeScene2GameServerParam make_server_scene_param;
        make_server_scene_param.scene_config_id_ = scene_config_id;
        make_server_scene_param.server_entity_ = new_server_entity;
        server_scene_enitity = MakeScene2GameServer(reg, make_server_scene_param);
    }
    else
    {
        server_scene_enitity = new_server_scene.scene_id(param.scene_config_id_);
    }

    if (entt::null == server_scene_enitity)
    {
        return;
    }

    LeaveSceneParam leave_param;
    leave_param.leave_entity_ = compel_entity;
    LeaveScene(reg, leave_param);

    EnterSceneParam enter_param;
    enter_param.enter_entity_ = compel_entity;
    enter_param.scene_entity_ = server_scene_enitity;
    EnterScene(reg, enter_param);
}

}//namespace master

