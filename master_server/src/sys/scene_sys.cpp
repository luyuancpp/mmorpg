#include "scene_sys.hpp"

#include "src/game_logic/comp/server_list.hpp"

namespace master
{
void EnterScene(entt::registry& reg, const EnterSceneParam& param)
{
    auto& scene_entity = param.scene_entity_;
    auto& player_entities =  reg.get<common::PlayerEntities>(scene_entity);
    player_entities.emplace(param.enter_entity_);
    reg.emplace<common::SceneEntityId>(param.enter_entity_, scene_entity);
}

void LeaveScene(entt::registry& reg, const LeaveSceneParam& param)
{
    auto& leave_entity = param.leave_entity_;
    auto& scene_entity = reg.get<common::SceneEntityId>(leave_entity);
    auto& player_entities = reg.get<common::PlayerEntities>(scene_entity.scene_entity_);
    player_entities.erase(leave_entity);
    reg.remove<common::SceneEntityId>(leave_entity);
}
}//namespace master

