#include "spatial/system/view.h"
#include "proto/common/component/actor_comp.pb.h"
#include "engine/core/type_define/type_define.h"
#include <thread_context/registry_manager.h>

extern EntityUnorderedMap entitiesToNotifyEntry;
extern EntityUnorderedMap entitiesToNotifyExit;

bool ViewSystem::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entity) {
    return true; // Mock behavior
}

void ViewSystem::FillActorCreateMessageInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& actorCreateMessage) {
    entitiesToNotifyEntry.emplace(observer, entity);
}

bool ViewSystem::IsWithinViewRadius(entt::entity observer, entt::entity entrant, double view_radius)
{
    return true;
}

bool ViewSystem::IsWithinViewRadius(entt::entity observer, entt::entity entrant)
{
    return true;
}

bool ViewSystem::CanSee(entt::entity observer, entt::entity target)
{
    // Mock: delegates to IsWithinViewRadius (no stealth in test by default).
    return IsWithinViewRadius(observer, target);
}

bool ViewSystem::IsStealthed(entt::entity entity)
{
    return false;
}

double ViewSystem::GetMaxViewRadius(entt::entity observer)
{
    return 0;
}

void ViewSystem::LookAtPosition(entt::entity entity, const Vector3& pos) {

}

