#include "actor_state_attribute_sync_util.h"

#include "component/actor_status_comp.pb.h"
#include "thread_local/storage.h"

void ActorStateAttributeSyncUtil::Initialize(){
    
}

void ActorStateAttributeSyncUtil::InitializeActorComponents(const entt::entity entity){
    tls.registry.emplace<CalculatedAttributesPBComponent>(entity);
    tls.registry.emplace<DerivedAttributesPBComponent>(entity);
}
