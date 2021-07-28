#include "mission_sys.hpp"

#include "src/game_logic/comp/mission.hpp"

#include "comp.pb.h"

namespace common
{
void CompleteAllMission(entt::registry& reg, entt::entity e, uint32_t op)
{
    auto& mm = reg.get<MissionMap>(e);
    auto& cm = reg.get<CompleteMissionsId>(e);
    for (auto& meit : mm)
    {
        auto& mission = reg.get<Mission>(meit.second);
        cm.mutable_missions()->insert({mission.id(), true});
        reg.remove<Mission>(meit.second);
    }
 
    reg.remove<MissionMap>(e);
}

}//namespace common

