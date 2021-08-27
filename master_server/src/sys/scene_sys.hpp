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
    entt::entity leave_entity_{};
    uint32_t op_{ 1 };
};

struct GetWeightRoundRobinSceneParam
{
    uint32_t scene_config_id_{0};
    uint32_t op_{ 0 };
};

void EnterScene(entt::registry& reg, const EnterSceneParam& param);

void LeaveScene(entt::registry& reg, const LeaveSceneParam& param);

entt::entity GetWeightRoundRobinMainSceneEntity(entt::registry& reg, const GetWeightRoundRobinSceneParam& param);

}//namespace master

#endif//MASTER_SRC_SCENE_SYS_SCENE_SYS_HPP_
