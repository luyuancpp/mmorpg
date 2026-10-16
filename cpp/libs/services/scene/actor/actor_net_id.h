#pragma once

#include <cstdint>

#include "entt/src/entt/entity/registry.hpp"

// 仅供客户端识别本次在线 actor 的不透明号,非玩家 GUID、非 EnTT 句柄,不持久化。
struct ActorNetIdComp
{
    uint64_t value{0};
};

namespace actor_net_id {
// 反查索引属于本 registry;只在其所属 EventLoop 线程使用。重复安装不重置号源或索引。
void Install(entt::registry& actors);

// 创建 actor 时调用(未来 NPC/怪物生成入口同样必须调用),已有号时幂等返回。
// 号源为 node32 + seq32,本进程内不复用;未安装、node 为0或序号耗尽时返回0。
uint64_t Assign(entt::registry& actors, entt::entity actor);

// 只读边界:缺少可信映射返回0/null,不补组件、不修索引。
uint64_t Of(const entt::registry& actors, entt::entity actor);
entt::entity Resolve(const entt::registry& actors, uint64_t netId);
} // namespace actor_net_id
