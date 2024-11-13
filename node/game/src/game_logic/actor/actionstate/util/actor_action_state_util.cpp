#include "actor_action_state_util.h"

#include "actionstate_config.h"
#include "common_error_tip.pb.h"
#include "component/actor_comp.pb.h"
#include "game_logic/actor/actionstate/constants/actor_state_constants.h"
#include "thread_local/storage.h"

void ActorActionStateUtil::InitializeActorComponents(entt::entity entity)
{
    tls.registry.emplace<ActorStatePbComponent>(entity);
}

uint32_t ActorActionStateUtil::TryAction(entt::entity actorEntity, uint32_t action)
{
    auto [actionStateTable, result] = GetActionStateTable(action);
    if (nullptr == actionStateTable)
    {
        return result;
    }

    for (const auto&  actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
         const auto& actorState : actorStatePbComponent.state_list() | std::views::keys){
        if (actorState >= static_cast<uint32_t>(actionStateTable->state_size()))
        {
            continue;
        }

        if (auto& state = actionStateTable->state(static_cast<int32_t>(actorState));
            state.state_flag() == kMutualExclusion){
            return  state.state_tip();
        }else if (state.state_flag() == kAllow){
            continue;
        }else if (state.state_flag() == kInterruptAndExecute)
        {
            continue;
        }
    }

    return kOK;
}

uint32_t ActorActionStateUtil::CanDoAction(entt::entity actorEntity, uint32_t action)
{
    return kOK;
}

bool ActorActionStateUtil::InState(entt::entity actorEntity, uint32_t state)
{
    const auto&  actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    
    if (state >= kActorStateMax )
    {
        return  false;    
    }

    return actorStatePbComponent.state_list().contains(state);
}

uint32_t ActorActionStateUtil::EnterState(const entt::entity actorEntity, uint32_t state)
{
    auto&  actorStatePbComponent = tls.registry.get<ActorStatePbComponent>(actorEntity);
    
    if (state > kActorStateMax || actorStatePbComponent.state_list().contains(state))
    {
        return  kInvalidParameter;    
    }

    actorStatePbComponent.mutable_state_list()->emplace(std::make_pair(state, true));
    
    return kOK;
}

uint32_t ActorActionStateUtil::InterruptAction(entt::entity actorEntity, uint32_t state)
{
    return kOK;
}
