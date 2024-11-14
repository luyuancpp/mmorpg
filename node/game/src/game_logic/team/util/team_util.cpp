#include "team_util.h"

#include "common_error_tip.pb.h"
#include "game_logic/actor/actionstate/constants/actor_state_constants.h"
#include "game_logic/actor/actionstate/util/actor_action_state_util.h"

uint32_t TeamUtil::TryCreateTeam(entt::entity actorEntity)
{
    return kOK;
}

uint32_t TeamUtil::InitTeamInfo(entt::entity actorEntity)
{
    ActorActionStateUtil::EnterState(actorEntity, kActorStateTeamFollow);
    return kOK;
}
