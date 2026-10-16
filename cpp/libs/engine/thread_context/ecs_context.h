#pragma once

#include <unordered_map>
#include "core/utils/registry/game_registry.h"
#include "core/utils/registry/scene_entity.h"
#include "entt/src/entt/signal/dispatcher.hpp"
#include "core/type_define/type_define.h"
#include "core/type_define/player_guid_comp.h"

using PlayerListMap = std::unordered_map<Guid, entt::entity>;
using SessionPlayerIndex = std::unordered_map<SessionId, Guid>;

// 每个线程各有一份 ECS 上下文,寿命随所属线程,跨 RPC 调用保留。
// 生产中的世界、事件与路由索引只由节点主 EventLoop 线程访问,不跨线程共享。
struct EcsContext
{
    // 不变量:actorRegistry 里任何实体的整数值都不是 0。
    // 对外 actor 号由 ActorNetIds 独立分配;此约束仅兼容仍存本地整数的 SkillContext/Buff:
    // 它们以 0 表示「没有目标 / 没有施法者」(uint64 默认值)。全新 registry 第一次 create() 出来的
    // 正是 index 0、version 0 的实体,整数值就是 0 —— 本节点第一个登录的玩家会和
    // 「没有目标」撞在一起。这里在线程启动时 create 再 destroy 一次,让 0 号槽位的
    // version 变成 1:之后复用这个槽位的实体整数是 1 << 32,valid(entity{0}) 恒为 false。
    // registry.clear() 只会继续抬高 version,不会让这条不变量失效。
    EcsContext();

    // --- Entity Registries ---
    entt::registry actorRegistry;
    SceneRegistry sceneRegistry;

    // --- Event Dispatcher ---
    entt::dispatcher dispatcher;

    // --- Player Index ---
    PlayerListMap playerList;

    // scene 的「gate 会话号 -> 玩家号」路由索引,不是实体组件。
    // 入场预登记与 UpdateSessionDetail 写入,退出收尾与陈旧会话清理时擦除。
    // 与 playerList 同属主 EventLoop 线程,由 Clear() 一并清空。
    SessionPlayerIndex sessionPlayers;

    // --- Player Lookup ---
    entt::entity GetPlayer(Guid playerUid);

    // --- Lifecycle ---
    void Clear();

};

extern thread_local EcsContext tlsEcs;
