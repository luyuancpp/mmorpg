#include "actor_status_util.h"

#include "component/actor_status_comp.pb.h"
#include "thread_local/storage.h"

void ActorStatusUtil::Initialize(){
    
}

void ActorStatusUtil::InitializeActorComponents(const entt::entity entity){
    tls.registry.emplace<CalculatedAttributesPBComponent>(entity);
    tls.registry.emplace<DerivedAttributesPBComponent>(entity);
}
