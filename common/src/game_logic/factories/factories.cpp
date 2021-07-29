#include "factories.h"

#include "entt/src/entt/entity/registry.hpp"

#include "src/common_type/common_type.h"
#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_logic/comp/mission.hpp"
#include "src/random/random.h"
#include "src/return_code/return_notice_code.h"

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

entt::entity MakePlayerMissionMap(entt::registry& reg)
{
    auto e = MakeMissionMap(reg);
    reg.emplace<CompleteMissionsId>(e);
    reg.emplace<UI32PairSet>(e);
    return e;
}

uint32_t MakeMission(entt::registry& reg, entt::entity e, uint32_t id)
{
    auto cids = MissionJson::GetSingleton().Primary1KeyRow(id);
    if (nullptr == cids)
    {
        return RET_TABLE_ID_ERROR;
    }
    Mission m;
    for (int32_t i = 0; i < cids->condition_id_size(); ++i)
    {
        auto pcs = m.add_conditions();
        pcs->set_id(cids->condition_id(i));
    }
    if (cids->random_condition_pool_size() > 0)
    {
        auto i = Random::GetSingleton().Rand<int32_t>(0, cids->random_condition_pool_size() - 1);
        auto pcs = m.add_conditions();
        pcs->set_id(cids->random_condition_pool().Get(i));
    }
    auto ret = reg.get<MissionMap>(e).mutable_missions()->insert({ id, std::move(m) });
    return RET_OK;
}

uint32_t MakePlayerMission(entt::registry& reg, entt::entity e, uint32_t id)
{
    auto cids = MissionJson::GetSingleton().Primary1KeyRow(id);
    if (nullptr == cids)
    {
        return RET_TABLE_ID_ERROR;
    }
    if (cids->mission_type() > 0 && cids->mission_sub_type() > 0)
    {
        UI32PairSet::value_type p(cids->mission_type(), cids->mission_sub_type());
        auto& type_set = reg.get<UI32PairSet>(e);
        auto it = type_set.find(p);
        if (it != type_set.end())
        {
            return RET_TABLE_ID_ERROR;
        }
    }    
    return MakeMission(reg, e, id);
}

}//namespace common


