#include "actor_status_util.h"

#include "component/actor_status_comp.pb.h"
#include "thread_local/storage.h"

void ActorStatusUtil::InitializeActorComponents(entt::entity entity)
{
    tls.registry.emplace<ActorStatusPBComponent>(entity);
}
