#include "system/scene/view.h"

#include "type_define/type_define.h"
#include "thread_local/storage.h"

extern EntityUnorderedMap entitiesToNotifyEntry;
extern EntityUnorderedMap entitiesToNotifyExit;

bool ViewSystem::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entity) {
    return true; // Mock behavior
}

bool ViewSystem::ShouldSendPlayerEnterMessage(entt::entity observer, entt::entity entity) {
    
    return true; // Mock behavior
}

void ViewSystem::FillActorCreateMessageInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& actorCreateMessage) {
    entitiesToNotifyEntry.emplace(observer, entity);
}

void ViewSystem::HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver)
{
    entitiesToNotifyExit.emplace(observer, leaver);
}