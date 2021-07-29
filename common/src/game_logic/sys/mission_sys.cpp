#include "mission_sys.hpp"

#include "src/game_logic/comp/mission.hpp"
#include "src/game_logic/game_registry.h"

#include "comp.pb.h"

namespace common
{
void CompleteAllMission(entt::entity e, uint32_t op)
{
    auto& mm = reg().get<MissionMap>(e);
    auto& cm = reg().get<CompleteMissionsId>(e);
    for (auto& meit : mm.missions())
    {
        cm.mutable_missions()->insert({ meit.first, true});
    }
    reg().remove<MissionMap>(e);
}

}//namespace common

