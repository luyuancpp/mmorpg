#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

#include "player/system/travel_freeze_cap.h"

// 归属只在冻结快照的两张 SQL ACK 齐备后才可切换。Lua 的唯一写入是最后的 handoff SET,
// 因而 0/-1/ERROR 都能证明本次未写标记;连接断开或不认识的回包仍属于结果未知。
namespace handoff_sql_barrier
{
    inline constexpr std::chrono::milliseconds kRetryInterval{250};

    inline travel_freeze_cap::Clock::time_point RetryDueAt(
        travel_freeze_cap::Clock::time_point frozenAt, travel_freeze_cap::Clock::time_point now)
    {
        return std::min(now + kRetryInterval, frozenAt + travel_freeze_cap::kDispatchWindow);
    }

    // timer 只认原冻结代次,退出、组件销毁重建或已有另一条标记命令都使它失效。
    inline bool IsCurrentWait(travel_freeze_cap::Clock::time_point expectedFrozenAt,
        travel_freeze_cap::Clock::time_point currentFrozenAt, uint64_t requestedAtMs, bool exiting)
    {
        return !exiting && expectedFrozenAt == currentFrozenAt && requestedAtMs == 0;
    }

    // KEYS=旧 owner / save_barrier / 旧 handoff;ARGV=期望 owner / 旧格式标记原文 / 固定 TTL。
    // 同实例原子读取并 SET;先验所有 TYPE/版本,不解析 protobuf,不铸造或采纳归属代次。
    inline constexpr const char* kPublishMarkLua = R"(
        local function gt(a, b)
            for i = 1, #a do
                local x, y = string.byte(a, i), string.byte(b, i)
                if x ~= y then return x > y end
            end
            return false
        end
        local function is_uint(s)
            if type(s) ~= 'string' or #s == 0 or #s > 20 or string.sub(s, 1, 1) == '0'
                or not string.match(s, '^%d+$') then return false end
            return #s < 20 or not gt(s, '18446744073709551615')
        end
        local function is_version(s)
            if type(s) ~= 'string' or #s ~= 41 or string.sub(s, 21, 21) ~= ':'
                or not string.match(s, '^%d+:%d+$') then return false end
            return not gt(string.sub(s, 1, 20), '18446744073709551615')
                and not gt(string.sub(s, 22, 41), '18446744073709551615')
                and string.sub(s, 22, 41) ~= string.rep('0', 20)
        end
        if #KEYS ~= 3 or #ARGV ~= 3 or KEYS[1] == KEYS[2] or KEYS[1] == KEYS[3] or KEYS[2] == KEYS[3] then
            return redis.error_reply('HANDOFF_SQL invalid keys or arguments')
        end
        for i = 1, 3 do
            if #KEYS[i] == 0 then return redis.error_reply('HANDOFF_SQL empty key') end
        end
        local owner_type = redis.call('TYPE', KEYS[1]).ok
        local barrier_type = redis.call('TYPE', KEYS[2]).ok
        local mark_type = redis.call('TYPE', KEYS[3]).ok
        if owner_type ~= 'string' or barrier_type ~= 'hash' or (mark_type ~= 'none' and mark_type ~= 'string') then
            return redis.error_reply('HANDOFF_SQL invalid key type')
        end
        local marker_owner, marker_time = string.match(ARGV[2], '^(%d+):(%d+)$')
        if not is_uint(ARGV[1]) or marker_owner ~= ARGV[1] or not is_uint(marker_time) or ARGV[3] ~= '300' then
            return redis.error_reply('HANDOFF_SQL invalid marker')
        end
        local owner = redis.call('GET', KEYS[1])
        local pending = redis.call('HGET', KEYS[2], 'pending')
        local first = redis.call('HGET', KEYS[2], 'player_database')
        local second = redis.call('HGET', KEYS[2], 'player_database_1')
        if not is_uint(owner) or not is_version(pending)
            or (first and not is_version(first)) or (second and not is_version(second)) then
            return redis.error_reply('HANDOFF_SQL invalid stored version')
        end
        if owner ~= ARGV[1] then return -1 end
        if string.sub(pending, 1, 20) ~= string.rep('0', 20 - #owner) .. owner then
            return redis.error_reply('HANDOFF_SQL pending owner mismatch')
        end
        if first ~= pending or second ~= pending then return 0 end
        redis.call('SET', KEYS[3], ARGV[2], 'EX', ARGV[3])
        return 1
    )";
} // namespace handoff_sql_barrier
