#ifndef COMMON_SRC_GAME_LOGIC_GAME_ENTITY_MISSION_FACTORIES_H_
#define COMMON_SRC_GAME_LOGIC_GAME_ENTITY_MISSION_FACTORIES_H_

#include "google/protobuf/repeated_field.h"

#include "entt/src/entt/entity/fwd.hpp"
#include "entt/src/entt/entity/entity.hpp"

#include "src/common_type/common_type.h"

namespace common
{

struct MakeMissionParam
{
    using ConditionV = ::google::protobuf::RepeatedField<::google::protobuf::uint32 >;
    MakeMissionParam(
        entt::entity e,
        uint32_t mision_id,
        const ConditionV* condition_id)
        : e_(e),
        mission_id_(mision_id),
        condition_id_(condition_id) {}
    MakeMissionParam(
        entt::entity e,
        uint32_t mision_id,
        const ConditionV& condition_id)
        : e_(e), 
        mission_id_(mision_id),
        condition_id_(&condition_id) {}

    entt::entity e_{entt::null};
    uint32_t mission_id_{ 0 };
    const ConditionV* condition_id_{nullptr};
};

struct MakePlayerMissionParam
{
    MakePlayerMissionParam(
        entt::entity e,
        uint32_t mision_id)
        : e_(e),
        mission_id_(mision_id)
    {}

    entt::entity e_{ entt::null };
    uint32_t mission_id_{ 0 };
};

}//namespace common

#endif // !COMMON_SRC_GAME_LOGIC_GAME_ENTITY_MISSION_FACTORIES_H_
