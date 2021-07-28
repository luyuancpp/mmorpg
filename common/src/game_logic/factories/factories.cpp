#include "factories.h"

#include "entt/src/entt/entity/registry.hpp"

#include "src/game_config/generator/json_cpp/condition_json.h"
#include "src/game_config/generator/json_cpp/mission_json.h"

#include "comp.pb.h"

namespace common
{
entt::entity MakeMission(entt::registry& reg, uint32_t id)
{
    auto e = reg.create();
    auto& ms =  reg.emplace<Mission>(e);
    ms.set_id(id); 
    auto& cs =  reg.emplace<Conditions>(e);
    auto cids = MissionJson::GetSingleton().Primary1KeyRow(id);
    if (nullptr != cids)
    {
        for (int32_t i = 0; i < cids->condition_id_size(); ++i)
        {
            auto pcs = cs.add_conditions();
            pcs->set_id(cids->condition_id(i));
        }        
    }
    return e;
}

}//namespace common


