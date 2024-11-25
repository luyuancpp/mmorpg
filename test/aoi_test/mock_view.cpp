#include "game_logic/scene/util/view_util.h"
#include "logic/component/actor_comp.pb.h"
#include "thread_local/storage.h"
#include "type_define/type_define.h"

extern EntityUnorderedMap entitiesToNotifyEntry;
extern EntityUnorderedMap entitiesToNotifyExit;

bool ViewUtil::ShouldSendNpcEnterMessage(entt::entity observer, entt::entity entity) {
    return true; // Mock behavior
}

void ViewUtil::FillActorCreateMessageInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& actorCreateMessage) {
    entitiesToNotifyEntry.emplace(observer, entity);
}

void ViewUtil::HandlePlayerLeaveMessage(entt::entity observer, entt::entity leaver)
{
    entitiesToNotifyExit.emplace(observer, leaver);
}

bool ViewUtil::IsWithinViewRadius(entt::entity observer, entt::entity entrant, double view_radius)
{
    return true;
}

bool ViewUtil::IsWithinViewRadius(entt::entity observer, entt::entity entrant)
{
    return true;
}

double ViewUtil::GetMaxViewRadius(entt::entity observer)
{
    return 0;
}

void ViewUtil::LookAtPosition(entt::entity entity, const Vector3& pos) {

}

