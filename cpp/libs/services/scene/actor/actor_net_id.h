#pragma once

#include <cstdint>

#include "entt/src/entt/entity/registry.hpp"

namespace actor_net_id {
// 反查索引属于本 registry;只在其所属 EventLoop 线程使用。重复安装不重置号源或索引。
void Install(entt::registry& actors);

// 创建 actor 时调用(未来 NPC/怪物生成入口同样必须调用),已有号时幂等返回。
// 号为 node_id<<32|seq，同时在线节点间唯一，本进程内不复用。
// 进程重启或 node_id 回收后，同一 node_id 从 seq=1 重新发号；客户端换场景、重连、换节点时清空 actor 表。
// 此号是客户端本次在线 actor 的不透明标识，非玩家 GUID、非 EnTT 句柄，不持久化。
// 未安装、node 为0或序号耗尽时返回0。
uint64_t Assign(entt::registry& actors, entt::entity actor);

// 只读边界：组件与反查索引由本模块创建、随实体销毁清理；查询缺失返回0/null，不补建或修复。
uint64_t Of(const entt::registry& actors, entt::entity actor);
entt::entity Resolve(const entt::registry& actors, uint64_t netId);
} // namespace actor_net_id
