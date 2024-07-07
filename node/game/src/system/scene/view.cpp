#include "view.h"

#include "constants/view.h"
#include "client_player_proto/scene_client_player.pb.h"
#include "component_proto/actor_comp.pb.h"
#include "component_proto/npc_comp.pb.h"
#include "Detour/DetourCommon.h"
#include "thread_local/storage.h"
#include "type_alias/actor.h"
#include "type_define/type_define.h"

void ViewSystem::Init()
{
    tls.global_registry.emplace<ActorCreateS2C>(global_entity());
}

bool ViewSystem::CheckSendNpcEnterMessage(entt::entity observer, entt::entity entrant)
{
    if (observer == entrant)
    {
        return false;
    }
    const bool is_observer_npc = tls.registry.any_of<Npc>(observer);
    const bool is_entrant_npc = tls.registry.any_of<Npc>(entrant);
    if (is_observer_npc && is_entrant_npc)
    {
        return false;
    }
    if (is_entrant_npc)
    {
        return  true;
    }
    // 如果突然失去视野需要重新刷新视野
    return true;
}

bool ViewSystem::CheckSendPlayerEnterMessage(entt::entity observer, entt::entity entrant)
{
    if (observer == entrant)
    {
        return false;
    }
    const bool is_observer_npc = tls.registry.any_of<Npc>(observer);
    const bool is_entrant_npc = tls.registry.any_of<Npc>(entrant);
    if (is_observer_npc && is_entrant_npc)
    {
        return false;
    }
    if (is_entrant_npc)
    {
        return  true;
    }
    
    // 如果突然失去视野需要重新刷新视野
    double view_radius = kViewRadius;
    if (const auto observer_view_radius = tls.registry.try_get<ViewRadius>(observer);
        nullptr != observer_view_radius)
    {
        view_radius = observer_view_radius->radius();
    }
    
    auto observer_transform = tls.registry.try_get<Transform>(observer);
    auto entrant_transform = tls.registry.try_get<Transform>(entrant);
    if (nullptr == observer_transform || nullptr == entrant_transform)
    {
        return false;
    }
    dtReal observer_location[] =
        {observer_transform->location().x(), observer_transform->location().y(), observer_transform->location().z()};
    dtReal entrant_location[] =
    {entrant_transform->location().x(), entrant_transform->location().y(), entrant_transform->location().z()};

    if (dtVdist(observer_location, entrant_location) > view_radius)
    {
        return false;
    }

    //检测优先关注列表
    
    return true;
}

void ViewSystem::FillActorCreateS2CInfo(entt::entity entrant)
{
    const auto actor_info = tls_actor_create_s2c.mutable_actor_list()->Add();
    actor_info->set_entity(entt::to_integral(entrant));
    if (const auto entrant_transform = tls.registry.try_get<Transform>(entrant);
        nullptr != entrant_transform)
    {
        actor_info->mutable_transform()->CopyFrom(*entrant_transform);
    }
    if (const auto guid = tls.registry.try_get<Guid>(entrant);
        nullptr != guid)
    {
        actor_info->set_guid(*guid);
    }
}
