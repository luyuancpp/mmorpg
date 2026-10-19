package saveoutbox

import (
	"shared/savebarrier"

	"github.com/redis/go-redis/v9"
)

// 单实例 Redis 前提与父快照提交 Lua 相同。租期只分摊重投工作，不替代 SQL 围栏。
//
// 补投节奏(审计 go-db-pipeline#1)全部记在 barrier hash 上、按意图版本计：
//   - 首次看到某版本只登记(outbox_attempt_version / outbox_attempts=0)，FirstReplayAfter 之后才第一次补投，
//     给正常 Kafka 投递留时间，不在消费积压时把每笔存盘都再投一遍；
//   - 第 n 次补投后等待 min(RetryAfter·2^(n-1), MaxRetryAfter)+抖动；达到 MaxAttempts 后不再补投(返回 4，转告警)；
//   - go/db 把该版本判为确定性拒写时写 outbox_terminal，补投见到即停止(返回 3)；
//   - go/db 最近 InflightWindow 内把该玩家的写延后进重试(outbox_retry_ms)时只推迟、不计次数(返回 6)。
//
// 终态、耗尽、损坏的意图都保留原字节与索引成员，按 RecheckAfter 复查；新版本存盘(C++ 改写 outbox_version)或两表 ACK 齐全
// 才离开这些状态。这些字段只由 go/db 读写；C++ 父快照 Lua、login 冷加载只读各自的固定字段，不受影响。
//
// 返回：0=未到期或已被其他 worker 推迟；1=取得原始意图与本次尝试序号；2=两表已提交并清理；-1=损坏或缺意图，保留；
// 3=终态(附原因)；4=已达尝试上限(附次数)；5=首次登记，未补投；6=有在途重试，推迟。
var claimScript = redis.NewScript(savebarrier.LuaVersionHelpers + `
local h, q = KEYS[1], KEYS[2]
if h == q then return redis.error_reply('save outbox keys must differ') end
local ht, qt = redis.call('TYPE', h).ok, redis.call('TYPE', q).ok
if (ht ~= 'none' and ht ~= 'hash') or (qt ~= 'none' and qt ~= 'zset') then
    return redis.error_reply('save outbox invalid key type')
end
local now, grace, base, cap = tonumber(ARGV[2]), tonumber(ARGV[3]), tonumber(ARGV[4]), tonumber(ARGV[5])
local max_attempts, jitter, window, recheck = tonumber(ARGV[6]), tonumber(ARGV[7]), tonumber(ARGV[8]), tonumber(ARGV[9])
if not now or not grace or not base or not cap or not max_attempts or not jitter or not window or not recheck
    or now <= 0 or grace <= 0 or base <= 0 or cap < base or max_attempts < 1 or jitter < 0 or window <= 0 or recheck <= 0 then
    return redis.error_reply('save outbox invalid schedule')
end
local score = redis.call('ZSCORE', q, ARGV[1])
if not score or tonumber(score) > now then return {0} end
local v = redis.call('HMGET', h, 'pending', 'player_database', 'player_database_1',
    'outbox_version', 'outbox_topic', 'outbox_player_database', 'outbox_player_database_1',
    'outbox_attempt_version', 'outbox_attempts', 'outbox_terminal', 'outbox_terminal_reason', 'outbox_retry_ms')
local strict_version = is_version(v[1]) and string.sub(v[1], 1, 20) ~= string.rep('0', 20)
    and string.sub(v[1], 22, 41) ~= string.rep('0', 20)
local complete = strict_version and v[2] == v[1] and v[3] == v[1]
local absent = not v[4] and not v[5] and not v[6] and not v[7]
local full = is_version(v[4]) and v[4] == v[1] and v[5] and #v[5] > 0
    and v[6] and #v[6] > 0 and v[7] and #v[7] > 0
if not strict_version or (v[2] and not is_version(v[2])) or (v[3] and not is_version(v[3]))
    or (not absent and not full) then
    redis.call('ZADD', q, now + recheck, ARGV[1])
    return {-1}
end
if complete then
    redis.call('HDEL', h, 'outbox_version', 'outbox_topic', 'outbox_player_database', 'outbox_player_database_1',
        'outbox_attempt_version', 'outbox_attempts', 'outbox_terminal', 'outbox_terminal_reason', 'outbox_retry_ms')
    redis.call('ZREM', q, ARGV[1])
    return {2}
end
if absent then
    redis.call('ZADD', q, now + recheck, ARGV[1])
    return {-1}
end
if v[10] then
    if v[10] == v[4] then
        redis.call('ZADD', q, now + recheck, ARGV[1])
        return {3, v[11] or ''}
    end
    redis.call('HDEL', h, 'outbox_terminal', 'outbox_terminal_reason')
end
local n = false
if v[8] == v[4] and v[9] and #v[9] <= 4 and string.match(v[9], '^%d+$') then n = tonumber(v[9]) end
if not n then
    redis.call('HSET', h, 'outbox_attempt_version', v[4], 'outbox_attempts', '0')
    redis.call('ZADD', q, now + grace + jitter, ARGV[1])
    return {5}
end
if n >= max_attempts then
    redis.call('ZADD', q, now + recheck, ARGV[1])
    return {4, n}
end
local delay = base
for i = 1, n do
    delay = delay * 2
    if delay >= cap then break end
end
if delay > cap then delay = cap end
if v[12] and #v[12] <= 16 and string.match(v[12], '^%d+$') and now - tonumber(v[12]) < window then
    redis.call('ZADD', q, now + delay + jitter, ARGV[1])
    return {6}
end
redis.call('HSET', h, 'outbox_attempts', tostring(n + 1))
redis.call('ZADD', q, now + delay + jitter, ARGV[1])
return {1, v[4], v[5], v[6], v[7], n + 1}
`)

var postponeInvalidMemberScript = redis.NewScript(`
if redis.call('TYPE', KEYS[1]).ok ~= 'zset' then return redis.error_reply('save outbox index type') end
local score = redis.call('ZSCORE', KEYS[1], ARGV[1])
if not score or tonumber(score) > tonumber(ARGV[2]) then return 0 end
redis.call('ZADD', KEYS[1], ARGV[3], ARGV[1])
return 1
`)

// 归属 home_zone 变化后，旧索引里的残留只能按原子版本快照迁到新topic所属zone。
// 原payload/epoch/seq不动；新父快照与迁移交错时CAS失败，留下一拍重读。
var migrateScript = redis.NewScript(`
local h, oldq, newq = KEYS[1], KEYS[2], KEYS[3]
if h == oldq or h == newq or oldq == newq then return redis.error_reply('save outbox migration keys must differ') end
if redis.call('TYPE', h).ok ~= 'hash' or redis.call('TYPE', oldq).ok ~= 'zset' then
    return redis.error_reply('save outbox migration source type')
end
local nt = redis.call('TYPE', newq).ok
if nt ~= 'none' and nt ~= 'zset' then return redis.error_reply('save outbox migration target type') end
if redis.call('HGET', h, 'pending') ~= ARGV[2]
    or redis.call('HGET', h, 'outbox_version') ~= ARGV[2]
    or redis.call('HGET', h, 'outbox_topic') ~= ARGV[3] then return 0 end
redis.call('ZADD', newq, 0, ARGV[1])
redis.call('ZREM', oldq, ARGV[1])
return 1
`)

// go/db 消费侧把某版本判为确定性拒写时调用：只在该版本仍是当前意图时打终态，旧版本的拒写不影响更新的意图。
// 不存在的 barrier 不创建(HSET 会凭空建出 hash，破坏「屏障只由父快照写者创建」的约定)。
var markTerminalScript = redis.NewScript(savebarrier.LuaVersionHelpers + `
if not is_version(ARGV[1]) or #ARGV[2] == 0 or #ARGV[2] > 64 then return redis.error_reply('save outbox terminal argument') end
local t = redis.call('TYPE', KEYS[1]).ok
if t == 'none' then return 0 end
if t ~= 'hash' then return redis.error_reply('save outbox terminal barrier type') end
if redis.call('HGET', KEYS[1], 'outbox_version') ~= ARGV[1] or redis.call('HGET', KEYS[1], 'pending') ~= ARGV[1] then return 0 end
redis.call('HSET', KEYS[1], 'outbox_terminal', ARGV[1], 'outbox_terminal_reason', ARGV[2])
return 1
`)

// go/db 消费侧每次把该玩家的严格写延后进重试时调用：只在仍有未完成意图时记下时刻(取较大值，容忍实例间时钟偏差)。
var noteRetryScript = redis.NewScript(`
if #ARGV[1] == 0 or #ARGV[1] > 16 or not string.match(ARGV[1], '^%d+$') then return redis.error_reply('save outbox retry stamp argument') end
local t = redis.call('TYPE', KEYS[1]).ok
if t == 'none' then return 0 end
if t ~= 'hash' then return redis.error_reply('save outbox retry stamp barrier type') end
if not redis.call('HGET', KEYS[1], 'outbox_version') then return 0 end
local old = redis.call('HGET', KEYS[1], 'outbox_retry_ms')
if old and #old <= 16 and string.match(old, '^%d+$') and tonumber(old) >= tonumber(ARGV[1]) then return 1 end
redis.call('HSET', KEYS[1], 'outbox_retry_ms', ARGV[1])
return 1
`)
