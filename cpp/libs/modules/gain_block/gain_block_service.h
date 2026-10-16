#pragma once

#include <cstdint>
#include <unordered_set>

// 各获得入口共用的全局封禁集合,在变更玩家资产之前检查。
// 当前存储为 thread_local,对当前 scene 线程的全部玩家生效;
// 多线程/多节点的规则传播由既有 GM 配置与热更新接线负责,本类不跨线程读写。
// 单人物品封禁没有生产写入口,已移除;货币的玩家级 blocked_types 仍由 CurrencySystem 检查。

class GainBlockService
{
public:
    enum class GainType : uint32_t
    {
        kCurrency = 0,
        kItem     = 1,
    };

    // ── Server-wide (global) blocks ──────────────────────────────────────

    // Block a specific ID globally for all players on this thread.
    static void BlockGlobal(GainType type, uint32_t id);

    // Unblock a previously globally blocked ID.
    static void UnblockGlobal(GainType type, uint32_t id);

    // Check whether an ID is globally blocked.
    static bool IsGloballyBlocked(GainType type, uint32_t id);

    // Return all globally blocked IDs of a given type (for GM query).
    static const std::unordered_set<uint32_t>& GlobalBlockedIds(GainType type);

    // Clear all global blocks (for server restart or GM reset).
    static void ClearAllGlobalBlocks();

    // 兼容各获得入口的全局查询门面;这里只查全局集合,不读取或创建玩家组件。
    static bool IsGainBlocked(GainType type, uint32_t id);
};
