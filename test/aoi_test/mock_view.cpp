#include "system/scene/view.h"

#include "type_define/type_define.h"
#include "thread_local/storage.h"

extern EntitySet entitiesToNotifyEntry;
extern EntitySet entitiesToNotifyExit;

bool ViewSystem::CheckSendNpcEnterMessage(entt::entity observer, entt::entity entity) {
    entitiesToNotifyEntry.emplace(observer);
    return true; // Mock behavior
}

bool ViewSystem::CheckSendPlayerEnterMessage(entt::entity observer, entt::entity entity) {
    entitiesToNotifyEntry.emplace(observer);
    return true; // Mock behavior
}

void ViewSystem::FillActorCreateS2CInfo(entt::entity entity) {
}

void ViewSystem::HandlerPlayerLeaveMessage(entt::entity observer, entt::entity leaver)
{
    entitiesToNotifyExit.emplace(leaver);
}