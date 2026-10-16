#include "interest.h"

#include <algorithm>
#include <entt/src/entt/entity/entity.hpp>

#include "spatial/comp/scene_node_scene_comp.h"
#include "spatial/constants/aoi_priority.h"

#include <modules/scene/comp/scene_comp.h>
#include <muduo/base/Logging.h>
#include <thread_context/ecs_context.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Find the entry with the lowest policy weight.  On tie, pick any.
static entt::entity FindLowestWeightEntity(const AoiListComp& comp,
                                           const AoiPriorityPolicy& policy)
{
    entt::entity worst = entt::null;
    uint8_t worstWeight = 255; // start high so anything is lower

    for (const auto& [entity, entry] : comp.entries)
    {
        const auto w = policy.GetWeight(entry.priority);
        if (w < worstWeight)
        {
            worstWeight = w;
            worst = entity;
        }
    }
    return worst;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::size_t InterestSystem::GetEffectiveCapacity(entt::entity watcher)
{
    // 三种容量/策略组件是稀疏可选配置,生产尚未接线。缺失表示使用默认值,不补建。
    // Client-reported desired count (per-entity).
    std::size_t clientCap = kAoiListCapacityDefault;
    if (const auto* clientComp = tlsEcs.actorRegistry.try_get<AoiClientCapacityComp>(watcher))
    {
        clientCap = std::clamp(clientComp->clientDesiredCount,
                               kAoiListCapacityMin, kAoiListCapacityMax);
    }

    // Server pressure-adjusted ceiling (per-scene).
    std::size_t serverCap = kAoiListCapacityMax;
    if (const auto* sceneComp = tlsEcs.actorRegistry.try_get<SceneEntityComp>(watcher))
    {
        if (tlsEcs.sceneRegistry.valid(sceneComp->sceneEntity))
        {
            if (const auto* pressure = tlsEcs.sceneRegistry.try_get<ScenePressureComp>(sceneComp->sceneEntity))
            {
                serverCap = pressure->GetServerCapacity();
            }
        }
    }

    return std::min(clientCap, serverCap);
}

const AoiPriorityPolicy& InterestSystem::GetPriorityPolicy(entt::entity watcher)
{
    // ScenePriorityPolicyComp 仅是预留入口,生产不预挂;缺省即 OpenWorld 策略。
    if (const auto* sceneComp = tlsEcs.actorRegistry.try_get<SceneEntityComp>(watcher))
    {
        if (tlsEcs.sceneRegistry.valid(sceneComp->sceneEntity))
        {
            if (const auto* policyComp = tlsEcs.sceneRegistry.try_get<ScenePriorityPolicyComp>(sceneComp->sceneEntity))
            {
                return PolicyOf(policyComp->policy);
            }
        }
    }
    return kPolicyOpenWorld;
}

bool InterestSystem::AddAoiEntity(entt::entity watcher, entt::entity target, AoiPriority priority)
{
    if (watcher == entt::null || target == entt::null) return false;

    // 先校验再动表:已销毁的句柄(格子集合里的陈旧条目、调用方持有的过期实体)
    // 既不能当观察者,也不能被登记进别人的兴趣表 —— 前者会在空槽位上挂出孤儿组件,
    // 后者会把下发给客户端的实体号指向一个不存在的 actor。
    if (!tlsEcs.actorRegistry.valid(watcher) || !tlsEcs.actorRegistry.valid(target))
    {
        LOG_ERROR << "AddAoiEntity: invalid entity, watcher=" << entt::to_integral(watcher)
                  << " target=" << entt::to_integral(target);
        return false;
    }

    // 按需状态的唯一写入口:没有兴趣表表示尚无观察对象,不是初始化失败。
    // 先完成句柄与容量校验,只有下面确实插入第一个条目时才延迟创建。
    auto* comp = tlsEcs.actorRegistry.try_get<AoiListComp>(watcher);
    const auto& policy = GetPriorityPolicy(watcher);

    // If target already present, upgrade priority if the new tag has higher weight.
    if (comp != nullptr)
    {
        if (auto it = comp->entries.find(target); it != comp->entries.end())
        {
            if (policy.GetWeight(priority) > policy.GetWeight(it->second.priority))
            {
                it->second.priority = priority;
            }
            return true;
        }
    }

    // Room available — insert directly.
    const auto capacity = GetEffectiveCapacity(watcher);
    if (capacity == 0) return false;
    if (comp == nullptr)
    {
        // 此处延迟创建,Pin 与空间可见性写入都经本入口。
        comp = &tlsEcs.actorRegistry.emplace<AoiListComp>(watcher);
    }
    if (comp->entries.size() < capacity)
    {
        comp->entries.emplace(target, AoiEntry{priority});
        return true;
    }

    // At capacity: try to evict the lowest-weight entry if new one outranks it.
    const auto lowestEntity = FindLowestWeightEntity(*comp, policy);
    if (lowestEntity == entt::null) return false;

    const auto lowestWeight = policy.GetWeight(comp->entries.at(lowestEntity).priority);
    if (policy.GetWeight(priority) <= lowestWeight) return false; // not important enough

    comp->entries.erase(lowestEntity);
    comp->entries.emplace(target, AoiEntry{priority});
    return true;
}

void InterestSystem::RemoveAoiEntity(entt::entity watcher, entt::entity target)
{
    if (watcher == entt::null || target == entt::null) return;

    auto* comp = tlsEcs.actorRegistry.try_get<AoiListComp>(watcher);
    if (comp == nullptr) return;
    comp->entries.erase(target);
}

bool InterestSystem::PinAoiEntity(entt::entity watcher, entt::entity target)
{
    return AddAoiEntity(watcher, target, AoiPriority::kPinned);
}

void InterestSystem::UpgradePriority(entt::entity watcher, entt::entity target, AoiPriority priority)
{
    if (watcher == entt::null || target == entt::null) return;

    auto* comp = tlsEcs.actorRegistry.try_get<AoiListComp>(watcher);
    if (comp == nullptr) return;

    auto it = comp->entries.find(target);
    if (it == comp->entries.end()) return;

    const auto& policy = GetPriorityPolicy(watcher);
    if (policy.GetWeight(priority) > policy.GetWeight(it->second.priority))
    {
        it->second.priority = priority;
    }
}

void InterestSystem::DowngradePriority(entt::entity watcher, entt::entity target, AoiPriority from, AoiPriority to)
{
    if (watcher == entt::null || target == entt::null) return;

    auto* comp = tlsEcs.actorRegistry.try_get<AoiListComp>(watcher);
    if (comp == nullptr) return;

    auto it = comp->entries.find(target);
    if (it == comp->entries.end()) return;

    // 只认标签不认权重:条目已被别的来源改成其它标签(如 kPinned / kAttacker)时,
    // 那个来源才是它的所有者,这里不能越权回退。
    if (it->second.priority != from) return;
    it->second.priority = to;
}

