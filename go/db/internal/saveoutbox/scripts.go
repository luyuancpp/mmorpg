package saveoutbox

import (
	"shared/savebarrier"

	"github.com/redis/go-redis/v9"
)

// 单实例 Redis 前提与父快照提交 Lua 相同。租期只分摊重投工作，不替代 SQL 围栏。
// 返回0=已被其他worker推迟，1=取得原始意图，2=两表已提交并清理，-1=损坏保留。
var claimScript = redis.NewScript(savebarrier.LuaVersionHelpers + `
local h, q = KEYS[1], KEYS[2]
if h == q then return redis.error_reply('save outbox keys must differ') end
local ht, qt = redis.call('TYPE', h).ok, redis.call('TYPE', q).ok
if (ht ~= 'none' and ht ~= 'hash') or (qt ~= 'none' and qt ~= 'zset') then
    return redis.error_reply('save outbox invalid key type')
end
local now, until_ms = tonumber(ARGV[2]), tonumber(ARGV[3])
if not now or not until_ms or until_ms <= now then return redis.error_reply('save outbox invalid lease') end
local score = redis.call('ZSCORE', q, ARGV[1])
if not score or tonumber(score) > now then return {0} end
local v = redis.call('HMGET', h, 'pending', 'player_database', 'player_database_1',
    'outbox_version', 'outbox_topic', 'outbox_player_database', 'outbox_player_database_1')
local strict_version = is_version(v[1]) and string.sub(v[1], 1, 20) ~= string.rep('0', 20)
    and string.sub(v[1], 22, 41) ~= string.rep('0', 20)
local complete = strict_version and v[2] == v[1] and v[3] == v[1]
local absent = not v[4] and not v[5] and not v[6] and not v[7]
local full = is_version(v[4]) and v[4] == v[1] and v[5] and #v[5] > 0
    and v[6] and #v[6] > 0 and v[7] and #v[7] > 0
if not strict_version or (v[2] and not is_version(v[2])) or (v[3] and not is_version(v[3]))
    or (not absent and not full) then
    redis.call('ZADD', q, until_ms, ARGV[1])
    return {-1}
end
if complete then
    redis.call('HDEL', h, 'outbox_version', 'outbox_topic', 'outbox_player_database', 'outbox_player_database_1')
    redis.call('ZREM', q, ARGV[1])
    return {2}
end
redis.call('ZADD', q, until_ms, ARGV[1])
if absent then return {-1} end
return {1, v[4], v[5], v[6], v[7]}
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
