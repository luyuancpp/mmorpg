#include "game_logic/scene/system/view_system.h"
#include "logic/component/actor_comp.pb.h"
#include "thread_local/storage.h"
#include "type_define/type_define.h"

extern EntityUnorderedMap entitiesToNotifyEntry;
extern EntityUnorderedMap entitiesToNotifyExit;

bool ViewSystem::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entity) {
    return true; // Mock behavior
}

void ViewSystem::FillActorCreateMessageInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& actorCreateMessage) {
    entitiesToNotifyEntry.emplace(observer, entity);
}

void ViewSystem::HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver)
{
    entitiesToNotifyExit.emplace(observer, leaver);
}

bool ViewSystem::IsWithinViewRadius(entt::entity observer, entt::entity entrant, double view_radius)
{
    return true;
}

bool ViewSystem::IsWithinViewRadius(entt::entity observer, entt::entity entrant)
{
    return true;
}

double ViewSystem::GetMaxViewRadius(entt::entity observer)
{
    return 0;
}

void ViewSystem::LookAtPosition(entt::entity entity, const Vector3& pos) {

}

