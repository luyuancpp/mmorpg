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
    auto& cscene_entity = reg.get<common::SceneEntityId>(leave_entity);
    auto scene_entity = cscene_entity.scene_entity();
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

entt::entity GetWeightRoundRobinSceneEntity(entt::registry& reg, const GetWeightRoundRobinSceneParam& param)
{
    auto& scene_map = reg.get<common::Scenes>(scenes_entity());
    auto scene_config_id = param.scene_config_id_;
    if (!scene_map.HasSceneType(scene_config_id))
    {
        return entt::null;
    }
    for (auto e : reg.view<common::GameServerStatusNormal>())
    {

    }
    return entt::null;
}

}//namespace master

