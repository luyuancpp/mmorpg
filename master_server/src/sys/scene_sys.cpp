#include "scene_sys.hpp"

#include "src/game_logic/comp/server_list.hpp"

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
    ++(*p_server_data)->player_size_;
}

void LeaveScene(entt::registry& reg, const LeaveSceneParam& param)
{
    auto leave_entity = param.leave_entity_;
    auto& cscene_entity = reg.get<common::SceneEntityId>(leave_entity);
    auto scene_entity = cscene_entity.scene_entity_;
    auto& player_entities = reg.get<common::PlayerEntities>(scene_entity);
    player_entities.erase(leave_entity);
    reg.remove<common::SceneEntityId>(leave_entity);
    auto p_server_data = reg.try_get<common::GameServerDataPtr>(scene_entity);
    if (nullptr == p_server_data)
    {
        return;
    }
    --(*p_server_data)->player_size_;
}

entt::entity GetWeightRoundRobinSceneEntity(entt::registry& reg, const GetWeightRoundRobinSceneParam& param)
{
    auto& scene_map = reg.get<common::Scenes>(param.scene_map_entity_);
    auto scene_config_id = param.scene_config_id_;
    auto it = scene_map.scenes_group_.find(scene_config_id);
    if (it == scene_map.scenes_group_.end())
    {
        return entt::null;
    }
    for (auto e : reg.view<common::GameServerComp>())
    {

    }
    return entt::null;
}

}//namespace master

