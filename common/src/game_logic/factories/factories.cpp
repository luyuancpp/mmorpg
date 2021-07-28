#include "factories.h"

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_logic/comp/mission.hpp"

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

}//namespace common


