#include "combat_state_util.h"

#include "proto/logic/component/actor_combat_state_comp.pb.h"
#include "thread_local/storage.h"

void CombatStateUtil::InitializeActorComponents(entt::entity entity)
{
    tls.registry.emplace<CombatStateListPbComponent>(entity);
}

void CombatStateUtil::AddSilence(const SilenceAddedPbEvent& event)
{
}

void CombatStateUtil::RemoveSilence(const SilenceRemovedPbEvent& event)
{
}
