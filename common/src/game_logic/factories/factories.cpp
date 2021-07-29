#include "factories.h"

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/random/random.h"

#include "comp.pb.h"

namespace common
{

entt::entity MakeMissionMap(entt::registry& reg)
{
    auto e = reg.create();
    reg.emplace<MissionMap>(e);
    reg.emplace<TypeMissionEntityMap>(e);
    return e;
}

entt::entity MakePlayerMission(entt::registry& reg)
{
    auto e = MakeMissionMap(reg);
    reg.emplace<CompleteMissionsId>(e);
    return e;
}

entt::entity MakeMission(entt::registry& reg, entt::entity e, uint32_t id)
{
    auto cids = MissionJson::GetSingleton().Primary1KeyRow(id);
    if (nullptr == cids)
    {
        return entt::null;
    }
    Mission m;
    for (int32_t i = 0; i < cids->condition_id_size(); ++i)
    {
        auto pcs = m.add_conditions();
        pcs->set_id(cids->condition_id(i));
    }

    auto ret = reg.get<MissionMap>(e).mutable_missions()->insert({ id, std::move(m) });
    return e;
}

entt::entity MakeRadomMission(entt::registry& reg, entt::entity e, uint32_t id)
{
    auto cids = MissionJson::GetSingleton().Primary1KeyRow(id);
    if (nullptr == cids)
    {
        return e;
    }
    MakeMission(reg, e, id);
    if (cids->random_condition_pool_size() > 0)
    {
        auto mit = reg.get<MissionMap>(e).mutable_missions()->find(id);
        auto i = Random::GetSingleton().Rand<int32_t>(0, cids->random_condition_pool_size() - 1);
        auto pcs = mit->second.add_conditions();
        pcs->set_id(cids->random_condition_pool().Get(i));
    }
    return e;
}

}//namespace common


