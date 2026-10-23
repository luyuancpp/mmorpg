#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "player/system/travel_freeze_cap.h"

// 归属只在冻结快照的两张 SQL ACK 齐备后才可切换。Lua 的唯一写入是最后的 handoff SET,
// 因而 0/-1/ERROR 都能证明本次未写标记;连接断开或不认识的回包仍属于结果未知。
namespace handoff_sql_barrier
{
    // 永久 hash 字段保存的是登记下限与本次资格，不是可由当前 Redis 身份补发的授权。
    // E|S|N|A|K|T：owner / 原场景实例 / 原 SQL admission generation / 本次加载稳定 nonce /
    // H持有、F冻结、R撤销 / 当前资格 nonce。数字比较在 Lua 中始终按十进制字符串进行。
    inline constexpr std::string_view kFenceField = "handoff_fence";
    enum class FenceState : uint8_t { kHeld, kFrozen, kRevoked };
    struct Fence
    {
        uint64_t ownerEpoch{0};
        std::string sceneInstance;
        uint64_t generation{0};
        std::string admissionToken;
        FenceState state{FenceState::kRevoked};
        std::string token;
    };

    inline bool IsFenceNonce(std::string_view value, bool allowZero = false)
    {
        return value.size() == 32 && (allowZero || value != std::string(32, '0'))
            && std::all_of(value.begin(), value.end(), [](char ch)
                { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); });
    }

    inline std::optional<uint64_t> ParseFenceNumber(std::string_view value, bool allowZero = false)
    {
        if (value.empty() || value.size() > 20 || (value.size() != 1 && value.front() == '0')) return std::nullopt;
        uint64_t number = 0;
        const auto parsed = std::from_chars(value.data(), value.data() + value.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || (!allowZero && number == 0))
            return std::nullopt;
        return number;
    }

    inline std::optional<Fence> ParseFence(std::string_view value)
    {
        std::array<std::string_view, 6> parts{};
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            const auto separator = value.find('|');
            if ((i + 1 < parts.size()) != (separator != std::string_view::npos)) return std::nullopt;
            parts[i] = value.substr(0, separator);
            if (separator != std::string_view::npos) value.remove_prefix(separator + 1);
        }
        const auto owner = ParseFenceNumber(parts[0]);
        const auto generation = ParseFenceNumber(parts[2], true);
        if (!owner || !generation || !IsFenceNonce(parts[3], true) || !IsFenceNonce(parts[5]) || parts[4].size() != 1
            || parts[1].size() > 256 || std::any_of(parts[1].begin(), parts[1].end(), [](unsigned char ch)
                { return ch <= 32 || ch == 127; })) return std::nullopt;
        FenceState state;
        switch (parts[4].front())
        {
        case 'H': state = FenceState::kHeld; break;
        case 'F': state = FenceState::kFrozen; break;
        case 'R': state = FenceState::kRevoked; break;
        default: return std::nullopt;
        }
        // 没有历史 admission 下限的恢复墓碑不是一名虚构持有者；同 E 一律不得从它登记。
        if (*generation == 0)
        {
            if (state != FenceState::kRevoked || !parts[1].empty() || parts[3] != std::string(32, '0'))
                return std::nullopt;
        }
        else if (parts[1].empty() || parts[3] == std::string(32, '0')) return std::nullopt;
        return Fence{*owner, std::string(parts[1]), *generation, std::string(parts[3]), state, std::string(parts[5])};
    }

    inline std::optional<std::string> EncodeFence(const Fence& fence)
    {
        char state;
        switch (fence.state)
        {
        case FenceState::kHeld: state = 'H'; break;
        case FenceState::kFrozen: state = 'F'; break;
        case FenceState::kRevoked: state = 'R'; break;
        default: return std::nullopt;
        }
        std::string value = std::to_string(fence.ownerEpoch) + "|" + fence.sceneInstance + "|"
            + std::to_string(fence.generation) + "|" + fence.admissionToken + "|" + state + "|" + fence.token;
        if (!ParseFence(value)) return std::nullopt;
        return value;
    }

    inline std::string NewFenceToken()
    {
        thread_local boost::uuids::random_generator generator;
        auto token = boost::uuids::to_string(generator());
        token.erase(std::remove(token.begin(), token.end(), '-'), token.end());
        return token;
    }

    inline std::optional<std::string> RotateFence(std::string_view original, FenceState state)
    {
        auto parsed = ParseFence(original);
        if (!parsed) return std::nullopt;
        parsed->state = state;
        parsed->token = NewFenceToken();
        return EncodeFence(*parsed);
    }

    // 所有脚本先验 TYPE 和完整原件，最后才写。单字段保留不可倒退的登记下限；
    // Redis Lua 错误不回滚此前写入，故校验函数不能混入任何副作用。
    inline constexpr const char* kFenceLuaHelpers = R"(
local function uint(s, zero)
    if type(s)~='string' or #s==0 or #s>20 or not string.match(s,'^%d+$') then return false end
    if s=='0' then return zero==true end
    if string.sub(s,1,1)=='0' then return false end
    return #s<20 or s<='18446744073709551615'
end
local function cmp(a,b)
    if #a~=#b then if #a>#b then return 1 else return -1 end end
    if a==b then return 0 elseif a>b then return 1 else return -1 end
end
local function nonce(s,zero)
    return type(s)=='string' and #s==32 and not string.find(s,'[^0-9a-f]')
        and (zero or s~=string.rep('0',32))
end
local function fence(s)
    if type(s)~='string' then return nil end
    local e,i,n,a,k,t=string.match(s,'^([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)|([^|]*)$')
    if not uint(e) or not uint(n,true) or not nonce(a,true) or not nonce(t)
        or #i>256 or string.find(i,'[%c%s]') or (k~='H' and k~='F' and k~='R') then return nil end
    if n=='0' then
        if k~='R' or i~='' or a~=string.rep('0',32) then return nil end
    elseif i=='' or a==string.rep('0',32) then return nil end
    return {e=e,i=i,n=n,a=a,k=k,t=t}
end
local function sameholder(a,b)
    return a.e==b.e and a.i==b.i and a.n==b.n and a.a==b.a
end
local function types3()
    if (#KEYS~=3 and #KEYS~=4) or KEYS[1]==KEYS[2] or KEYS[1]==KEYS[3] or KEYS[2]==KEYS[3] then return false end
    for _,key in ipairs(KEYS) do if key=='' then return false end end
    local b=redis.call('TYPE',KEYS[2]).ok
    local m=redis.call('TYPE',KEYS[3]).ok
    return redis.call('TYPE',KEYS[1]).ok=='string' and (b=='none' or b=='hash')
        and (m=='none' or m=='string') and (b=='none' or redis.call('PTTL',KEYS[2])==-1)
end
local function prior()
    local raw=redis.call('HGET',KEYS[2],'handoff_fence')
    if raw==false then return '',nil end
    local parsed=fence(raw)
    if not parsed then return nil,nil end
    return raw,parsed
end
local function version(s)
    return type(s)=='string' and #s==41 and string.sub(s,21,21)==':'
        and string.match(s,'^%d+:%d+$')~=nil
        and string.sub(s,1,20)<='18446744073709551615'
        and string.sub(s,22)<='18446744073709551615' and string.sub(s,22)~=string.rep('0',20)
end
local function marker(s,e)
    local owner,time=string.match(s or '','^(%d+):(%d+)$')
    return owner==e and uint(time)
end
)";

    // 原 SQL 已确认的入场上下文只能登记一次。同 E 的旧 N 即使晚读到新 prior 也不可接任；
    // 同 N 只认原 A/T 的完全幂等，不重复删除标记。缺 floor 的恢复墓碑拒同 E。
    inline const std::string kRegisterFenceLua = std::string(kFenceLuaHelpers) + R"(
if #ARGV~=3 or not types3() then return redis.error_reply('HANDOFF register shape') end
local candidate=fence(ARGV[2])
local raw,old=prior()
if not uint(ARGV[1]) or not candidate or candidate.k~='H' or candidate.e~=ARGV[1] or raw==nil then
    return redis.error_reply('HANDOFF register record') end
if redis.call('GET',KEYS[1])~=ARGV[1] then return -1 end
if raw==ARGV[2] then return 1 end
if raw~=ARGV[3] then return -2 end
if old then
    local order=cmp(candidate.e,old.e)
    if order<0 or (order==0 and (old.n=='0' or candidate.i~=old.i or cmp(candidate.n,old.n)<=0)) then return -2 end
end
local mark=redis.call('GET',KEYS[3])
if mark then
    local e,t=string.match(mark,'^(%d+):(%d+)$')
    if not uint(e) or not uint(t) or cmp(e,candidate.e)>0 then return redis.error_reply('HANDOFF register marker') end
end
redis.call('HSET',KEYS[2],'handoff_fence',ARGV[2])
if mark then redis.call('DEL',KEYS[3]) end
return 1
)";

    // 只确认登记时已经清过标记的原资格；旧 A2 回调不能再次删新持有者的标记。
    inline const std::string kConfirmFenceLua = std::string(kFenceLuaHelpers) + R"(
if #ARGV~=2 or not types3() then return redis.error_reply('HANDOFF confirm shape') end
local expected=fence(ARGV[2])
local raw=prior()
if not expected or expected.k~='H' or expected.e~=ARGV[1] or raw==nil then return redis.error_reply('HANDOFF confirm record') end
if redis.call('GET',KEYS[1])~=ARGV[1] or raw~=ARGV[2] then return -1 end
return 0
)";

    // 撤销先于解冻，即使 mark 从未写成也轮换资格。F 尚未抵达时也可从原 H 撤销；
    // 之后迟到的 H→F 命令必因 CAS 失败。异代返回 0 只说明旧资格已不可写，不授权修改当前实体。
    inline const std::string kRevokeFenceLua = std::string(kFenceLuaHelpers) + R"(
if #ARGV~=4 or not types3() then return redis.error_reply('HANDOFF revoke shape') end
local held,published,next=fence(ARGV[1]),fence(ARGV[2]),fence(ARGV[3])
local raw=prior()
if not held or not published or not next or raw==nil or held.k~='H'
    or not sameholder(held,published) or not sameholder(held,next)
    or (next.k~='H' and next.k~='R') or next.t==held.t or next.t==published.t then
    return redis.error_reply('HANDOFF revoke record') end
if raw~=ARGV[1] and raw~=ARGV[2] and raw~=ARGV[3] then return 0 end
local mark=redis.call('GET',KEYS[3])
local stamp=string.match(mark or '', '^%d+:(%d+)$')
redis.call('HSET',KEYS[2],'handoff_fence',ARGV[3])
if mark and ARGV[4]~='' and stamp==ARGV[4] then redis.call('DEL',KEYS[3]) end
return 1
)";

    // A1、疏散和放弃载入补写只消费原 H；不读当前 holder 给旧调用补授权。
    inline const std::string kReleaseMarkLua = std::string(kFenceLuaHelpers) + R"(
if #ARGV~=5 or not types3() then return redis.error_reply('HANDOFF release shape') end
local held,next=fence(ARGV[4]),fence(ARGV[5])
local raw=prior()
if not held or not next or raw==nil or held.k~='H' or next.k~='F' or not sameholder(held,next)
    or held.t==next.t or held.e~=ARGV[1] or not marker(ARGV[2],ARGV[1]) or ARGV[3]~='300' then
    return redis.error_reply('HANDOFF release record') end
if redis.call('GET',KEYS[1])~=ARGV[1] then return 0 end
if raw==ARGV[5] then return 1 end
if raw~=ARGV[4] then return 0 end
redis.call('HSET',KEYS[2],'handoff_fence',ARGV[5])
redis.call('SET',KEYS[3],ARGV[2],'EX',ARGV[3])
return 1
)";

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

    // 发布和资格转换是同一提交点：原 H -> 本轮 F，双 ACK 必须仍齐备。
    inline const std::string kPublishMarkLua = std::string(kFenceLuaHelpers) + R"(
if #ARGV~=5 or not types3() then return redis.error_reply('HANDOFF_SQL shape') end
local held,next=fence(ARGV[4]),fence(ARGV[5])
local raw=prior()
if not held or not next or raw==nil or held.k~='H' or next.k~='F' or not sameholder(held,next)
    or held.t==next.t or held.e~=ARGV[1] or not marker(ARGV[2],ARGV[1]) or ARGV[3]~='300' then
    return redis.error_reply('HANDOFF_SQL record') end
if redis.call('GET',KEYS[1])~=ARGV[1] then return -1 end
if raw==ARGV[5] then return 1 end
if raw~=ARGV[4] then return -2 end
local ack=redis.call('HMGET',KEYS[2],'pending','player_database','player_database_1')
if not version(ack[1]) or (ack[2] and not version(ack[2])) or (ack[3] and not version(ack[3]))
    or string.sub(ack[1],1,20)~=string.rep('0',20-#ARGV[1])..ARGV[1] then
    return redis.error_reply('HANDOFF_SQL stored version') end
if ack[2]~=ack[1] or ack[3]~=ack[1] then return 0 end
redis.call('HSET',KEYS[2],'handoff_fence',ARGV[5])
redis.call('SET',KEYS[3],ARGV[2],'EX',ARGV[3])
return 1
)";

    // outcome 原子撤资格再读证据。owner 已前进时保存旧 E 的 R floor，绝不凭读值铸造新资格。
    inline const std::string kJudgeOutcomeLua = std::string(kFenceLuaHelpers) + R"(
if #KEYS~=4 or #ARGV~=4 then return redis.error_reply('HANDOFF outcome shape') end
-- 将前三键映射为 owner/barrier/mark，第四键是只读 location。
local location=KEYS[4]
if redis.call('TYPE',location).ok~='none' and redis.call('TYPE',location).ok~='string' then
    return redis.error_reply('HANDOFF location type') end
if not types3() then return redis.error_reply('HANDOFF outcome keys') end
local held,published,next=fence(ARGV[1]),fence(ARGV[2]),fence(ARGV[3])
local raw=prior()
if not held or not published or not next or raw==nil or not sameholder(held,published)
    or not sameholder(held,next) or held.k~='H' or next.k~='R'
    or next.t==held.t or next.t==published.t then return redis.error_reply('HANDOFF outcome record') end
if raw~=ARGV[1] and raw~=ARGV[2] and raw~=ARGV[3] then return redis.error_reply('HANDOFF outcome superseded') end
local mark=redis.call('GET',KEYS[3])
local stamp=string.match(mark or '', '^%d+:(%d+)$')
redis.call('HSET',KEYS[2],'handoff_fence',ARGV[3])
if stamp==ARGV[4] then redis.call('DEL',KEYS[3]) end
return {redis.call('GET',KEYS[1]),redis.call('GET',location)}
)";

    // B4/B5 的唯一恢复口：原 R 原件与 location 整字节 CAS；B5 只采纳已交叉核过的 E+2。
    inline const std::string kResumeFenceLua = std::string(kFenceLuaHelpers) + R"(
if #KEYS~=4 or #ARGV~=4 then return redis.error_reply('HANDOFF resume shape') end
local location=KEYS[4]
local t=redis.call('TYPE',location).ok
if t~='none' and t~='string' then return redis.error_reply('HANDOFF resume location') end
if not types3() then return redis.error_reply('HANDOFF resume keys') end
local old,next=fence(ARGV[1]),fence(ARGV[2])
local raw=prior()
if not old or not next or raw==nil or old.k~='R' or next.k~='H'
    or old.i~=next.i or old.n~=next.n or old.a~=next.a or old.t==next.t
    or next.e~=ARGV[3] or cmp(next.e,old.e)<0 then return redis.error_reply('HANDOFF resume record') end
if redis.call('GET',KEYS[1])~=ARGV[3] or (redis.call('GET',location) or '')~=ARGV[4] then return 0 end
if raw==ARGV[2] then return 1 end
if raw~=ARGV[1] then return 0 end
redis.call('HSET',KEYS[2],'handoff_fence',ARGV[2])
return 1
)";

} // namespace handoff_sql_barrier
