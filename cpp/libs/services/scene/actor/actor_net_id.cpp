#include "actor_net_id.h"

#include <unordered_map>

#include "core/system/id_generator.h"
#include "muduo/base/Logging.h"

namespace {
struct ActorNetIdIndex
{
    std::unordered_map<uint64_t, entt::entity> byNetId;
};

void OnDestroyed(entt::registry& actors, entt::entity actor)
{
    auto* index = actors.ctx().find<ActorNetIdIndex>();
    const auto* id = actors.try_get<ActorNetIdComp>(actor);
    if (index == nullptr || id == nullptr) return;
    const auto it = index->byNetId.find(id->value);
    if (it != index->byNetId.end() && it->second == actor) index->byNetId.erase(it);
}
} // namespace

void actor_net_id::Install(entt::registry& actors)
{
    if (actors.ctx().contains<ActorNetIdIndex>()) return;
    actors.ctx().emplace<ActorNetIdIndex>();
    // 自由函数随组件池销毁,不持有宿主地址;ctx 的析构晚于池,不能在ctx中放会访问池的RAII连接。
    actors.on_destroy<ActorNetIdComp>().connect<&OnDestroyed>();
}

uint64_t actor_net_id::Assign(entt::registry& actors, entt::entity actor)
{
    auto* index = actors.ctx().find<ActorNetIdIndex>();
    if (!actors.valid(actor) || index == nullptr) {
        LOG_ERROR << "ActorNetIds: assignment refused, invalid actor or registry not installed";
        return 0;
    }
    if (const auto* current = actors.try_get<ActorNetIdComp>(actor)) {
        if (Resolve(actors, current->value) == actor) return current->value;
        LOG_ERROR << "ActorNetIds: assignment refused, inconsistent actor mapping";
        return 0;
    }
    uint64_t id = 0;
    if (!tlsIdGeneratorManager.actorNetIdGenerator.TryGenerateWithoutReuse(id)) {
        LOG_ERROR << "ActorNetIds: assignment refused, node not allocated or sequence exhausted";
        return 0;
    }
    if (!index->byNetId.emplace(id, actor).second) {
        LOG_ERROR << "ActorNetIds: assignment refused, generated id already exists";
        return 0;
    }
    actors.emplace<ActorNetIdComp>(actor, id);
    return id;
}

uint64_t actor_net_id::Of(const entt::registry& actors, entt::entity actor)
{
    if (!actors.valid(actor)) return 0;
    const auto* id = actors.try_get<ActorNetIdComp>(actor);
    return id != nullptr && Resolve(actors, id->value) == actor ? id->value : 0;
}

entt::entity actor_net_id::Resolve(const entt::registry& actors, uint64_t netId)
{
    if (netId == 0) return entt::null;
    const auto* index = actors.ctx().find<ActorNetIdIndex>();
    if (index == nullptr) return entt::null;
    const auto it = index->byNetId.find(netId);
    if (it == index->byNetId.end() || !actors.valid(it->second)) return entt::null;
    const auto* id = actors.try_get<ActorNetIdComp>(it->second);
    return id != nullptr && id->value == netId ? it->second : entt::null;
}
