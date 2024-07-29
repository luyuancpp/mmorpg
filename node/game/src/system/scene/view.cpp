#include "view.h"

#include "constants/view.h"
#include "muduo/base/Logging.h"
#include "proto/logic/client_player/scene_client_player.pb.h"
#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/component/npc_comp.pb.h"
#include "Detour/DetourCommon.h"
#include "thread_local/storage.h"
#include "type_alias/actor.h"
#include "type_define/type_define.h"

void ViewSystem::Init()
{
    // 在全局注册表中初始化角色创建和销毁的消息
    tls.globalRegistry.emplace<ActorCreateS2C>(global_entity());
    tls.globalRegistry.emplace<ActorDestroyS2C>(global_entity());
    tls.globalRegistry.emplace<ActorListCreateS2C>(global_entity());
    tls.globalRegistry.emplace<ActorListDestroyS2C>(global_entity());
}

// 检查是否发送 NPC 进入消息
bool ViewSystem::CheckSendNpcEnterMessage(entt::entity observer, entt::entity entrant)
{
    const bool is_observer_npc = tls.registry.any_of<Npc>(observer);
    const bool is_entrant_npc = tls.registry.any_of<Npc>(entrant);

    // 如果观察者和进入者都是 NPC，则不发送消息
    if (is_observer_npc && is_entrant_npc) {
        return false;
    }

    // 如果进入者是 NPC，发送消息
    if (is_entrant_npc) {
        return true;
    }

    // TODO: 如果突然失去视野需要重新刷新视野
    return true;
}

// 检查是否发送玩家进入消息
bool ViewSystem::CheckSendPlayerEnterMessage(entt::entity observer, entt::entity entrant)
{
    const bool is_observer_npc = tls.registry.any_of<Npc>(observer);
    const bool is_entrant_npc = tls.registry.any_of<Npc>(entrant);

    // 如果观察者和进入者都是 NPC，则不发送消息
    if (is_observer_npc && is_entrant_npc) {
        return false;
    }

    // 如果进入者是 NPC，发送消息
    if (is_entrant_npc) {
        return true;
    }

    // 如果突然失去视野需要重新刷新视野
    double view_radius = kMaxViewRadius;

    // 获取观察者的视野半径
    if (const auto observer_view_radius = tls.registry.try_get<ViewRadius>(observer)) {
        view_radius = observer_view_radius->radius();
    }

    // 获取观察者和进入者的位置信息
    const auto observer_transform = tls.registry.try_get<Transform>(observer);
    const auto entrant_transform = tls.registry.try_get<Transform>(entrant);

    // 如果位置信息为空，无法发送消息
    if (!observer_transform || !entrant_transform) {
        return false;
    }

    // 计算观察者和进入者之间的距离
    const dtReal observer_location[] = {
        observer_transform->location().x(),
        observer_transform->location().y(),
        observer_transform->location().z()
    };
    const dtReal entrant_location[] = {
        entrant_transform->location().x(),
        entrant_transform->location().y(),
        entrant_transform->location().z()
    };

    // 如果距离超出视野半径，则不发送消息
    if (dtVdist(observer_location, entrant_location) > view_radius) {
        return false;
    }

    // TODO: 检测优先关注列表

    return true;
}

// 填充角色创建消息的信息
void ViewSystem::FillActorCreateS2CInfo(entt::entity observer, entt::entity entity, ActorCreateS2C& createMessage)
{
    createMessage.set_entity(entt::to_integral(entity));

    // 填充角色的位置信息
    if (const auto entrant_transform = tls.registry.try_get<Transform>(entity)) {
        createMessage.mutable_transform()->CopyFrom(*entrant_transform);
    }

    // 填充角色的 GUID
    if (const auto guid = tls.registry.try_get<Guid>(entity)) {
        createMessage.set_guid(*guid);
    }
}

// 处理玩家离开消息
void ViewSystem::HandlerPlayerLeaveMessage(entt::entity observer, entt::entity leaver)
{
    // 处理玩家离开消息，目前未实现具体逻辑
    // 可根据具体需求补充实现
}
