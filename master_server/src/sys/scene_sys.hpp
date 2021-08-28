#ifndef MASTER_SRC_SYS_SCENE_SYS_HPP_
#define MASTER_SRC_SYS_SCENE_SYS_HPP_

#include "entt/src/entt/entity/registry.hpp"

namespace master
{
struct EnterSceneParam
{
    entt::entity scene_entity_{ entt::null };
    entt::entity enter_entity_{ entt::null };
    uint32_t op_{ 0 };
};

struct LeaveSceneParam
{
    entt::entity leave_entity_{ entt::null };
    uint32_t op_{ 1 };
};

struct GetWeightRoundRobinSceneParam
{
    uint32_t scene_config_id_{0};
    uint32_t op_{ 0 };
};

struct ServerPressureParam
{
    entt::entity server_entity_{ entt::null };
    uint32_t op_{ 0 };
};

void EnterScene(entt::registry& reg, const EnterSceneParam& param);

void LeaveScene(entt::registry& reg, const LeaveSceneParam& param);

entt::entity GetWeightRoundRobinMainScene(entt::registry& reg, const GetWeightRoundRobinSceneParam& param);

void ServerEnterPressure(entt::registry& reg, const ServerPressureParam& param);

void ServerEnterNoPressure(entt::registry& reg, const ServerPressureParam& param);

}//namespace master

#endif//MASTER_SRC_SCENE_SYS_SCENE_SYS_HPP_
