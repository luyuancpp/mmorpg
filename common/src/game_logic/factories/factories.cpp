#include "factories.h"

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_config/generator/json_cpp/mission_json.h"
#include "src/game_logic/comp/mission.hpp"

#include "comp.pb.h"

namespace common
{
entt::entity MakePlayer(entt::registry& reg)
{
    auto e = reg.create();
    reg.emplace<Vector3>(e);
    return e;
}

entt::entity MakeMissionMap(entt::registry& reg)
{
    auto e = reg.create();
    reg.emplace<MissionMap>(e);
    reg.emplace<CompleteMissionsId>(e);
    return e;
}

entt::entity MakeMission(entt::registry& reg, entt::entity parent_id, uint32_t id)
{
    auto cids = MissionJson::GetSingleton().Primary1KeyRow(id);
    if (nullptr == cids)
    {
        return entt::null;
    }
    auto e = reg.create();
    auto& ms = reg.emplace<Mission>(e);
    ms.set_id(id);
    auto& cs = reg.emplace<Conditions>(e);

    for (int32_t i = 0; i < cids->condition_id_size(); ++i)
    {
        auto pcs = cs.add_conditions();
        pcs->set_id(cids->condition_id(i));
    }
    reg.get<MissionMap>(parent_id).emplace(id, e);
    return e;
}

}//namespace common


