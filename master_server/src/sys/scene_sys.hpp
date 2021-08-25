#ifndef MASTER_SRC_SYS_SCENE_SYS_HPP_
#define MASTER_SRC_SYS_SCENE_SYS_HPP_

#include "entt/src/entt/entity/registry.hpp"

namespace master
{
struct EnterSceneParam
{
    entt::entity scene_entity_{};
    entt::entity enter_entity_{};
    uint32_t op_{ 0 };
};

struct LeaveSceneParam
{
    uint32_t op_{ 1 };
};

void EnterScene(entt::registry& reg, const EnterSceneParam& param);

void LeaveScene(entt::registry& reg, const LeaveSceneParam& param);
}//namespace master

#endif//MASTER_SRC_SCENE_SYS_SCENE_SYS_HPP_
