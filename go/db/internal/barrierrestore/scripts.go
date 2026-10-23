package barrierrestore

import (
	"github.com/redis/go-redis/v9"
	"shared/savebarrier"
)

// 所有检查先于第一次写。维护键只是本工具互斥及人工停写声明，业务写者不会自动读取它。
var restoreScript = redis.NewScript(savebarrier.LuaVersionHelpers + `
local function is_schema_gen(s)
    if type(s) ~= 'string' or #s == 0 or #s > 10 or not string.match(s,'^[1-9]%d*$') then return false end
    return #s < 10 or not version_gt(s,'4294967295')
end
if (#KEYS ~= 75 and #KEYS ~= 72) or #ARGV ~= 8 or not is_version(ARGV[3])
    or not is_schema_gen(ARGV[8]) then return redis.error_reply('restore arguments invalid') end
local schema_gen=ARGV[8]
if redis.call('TYPE',KEYS[1]).ok ~= 'string' or redis.call('GET',KEYS[1]) ~= ARGV[1]
    or redis.call('PTTL',KEYS[1]) ~= -1 then return redis.error_reply('permanent maintenance marker required') end
if redis.call('TYPE',KEYS[2]).ok ~= 'string' or redis.call('GET',KEYS[2]) ~= ARGV[2]
    or redis.call('PTTL',KEYS[2]) ~= -1 then return redis.error_reply('owner changed or expires') end
if #KEYS == 75 then
for i=73,74 do
    if i == 74 and ARGV[7] == '' then
        -- 2026-10-20 修复(r2-mz#1):计划显式允许的本区空落点(AllowHomeFallback)按 home 回退,要求落点记录此刻
        -- 仍不存在,与写屏障同一原子段复核;home 仍须是上一轮核过的永久原值。空串只可能来自已校验的计划。
        if redis.call('EXISTS',KEYS[i]) ~= 0 then return redis.error_reply('authoritative route changed or expires') end
    elseif redis.call('TYPE',KEYS[i]).ok ~= 'string' or redis.call('GET',KEYS[i]) ~= ARGV[i-67]
        or redis.call('PTTL',KEYS[i]) ~= -1 then return redis.error_reply('authoritative route changed or expires') end
end
if redis.call('EXISTS',KEYS[75]) ~= 0 then return redis.error_reply('zone merge fence exists') end
end
for i=3,7 do
    if redis.call('EXISTS',KEYS[i]) ~= 0 then return redis.error_reply('cache evidence or handoff still exists') end
end
local t=redis.call('TYPE',KEYS[8]).ok
if t ~= 'none' and t ~= 'hash' then return redis.error_reply('barrier wrong type') end
if t == 'hash' then
    if ARGV[5] ~= '1' then return redis.error_reply('barrier exists before SQL migration') end
    for _,field in ipairs(redis.call('HKEYS',KEYS[8])) do
        -- schema_writer 是 C++ 严格屏障写入的写方令牌(混版本持久字段保护 §5.2),只供 Lua 判"上次是不是本实体写的";
        -- schema_regressed 是 warn 下被判倒退的写方令牌(防回包丢失的证据)。两者都接受,写入时清掉(见文末)。
        if field ~= 'pending' and field ~= 'player_database' and field ~= 'player_database_1' and field ~= 'schema_gen'
            and field ~= 'schema_writer' and field ~= 'schema_regressed' then
            return redis.error_reply('barrier or outbox conflicts')
        end
    end
    if redis.call('PTTL',KEYS[8]) ~= -1
        or redis.call('HGET',KEYS[8],'pending') ~= ARGV[3]
        or redis.call('HGET',KEYS[8],'player_database') ~= ARGV[3]
        or redis.call('HGET',KEYS[8],'player_database_1') ~= ARGV[3] then
        return redis.error_reply('barrier or outbox conflicts')
    end
    -- 已有的 schema_gen 只是索引不是权威(混版本持久字段保护 §3.3):恢复以 SQL 为准,直接用 ARGV[8](两行戳最小值)
    -- 覆盖,不保留旧值、也不因旧值格式损坏而拒绝 —— 保留更高的旧值会让恢复后的第一次加载被判倒退(§13 第 8 条)。
end
for i=9,72 do
    local kind=redis.call('TYPE',KEYS[i]).ok
    if kind ~= 'none' and kind ~= 'zset' then return redis.error_reply('pending index wrong type') end
    if kind == 'zset' and redis.call('ZCARD',KEYS[i]) ~= 0 then return redis.error_reply('zone pending outbox not drained') end
end
if ARGV[4] == '1' then
    redis.call('HSET',KEYS[8],'pending',ARGV[3],'player_database',ARGV[3],'player_database_1',ARGV[3],'schema_gen',schema_gen)
    -- 恢复以 SQL 为准:清掉写方令牌与 warn 倒退证据,下一次任何写方的严格屏障存盘都重新做一次世代比较(fail-closed)。
    -- 合规流程里恢复时没有在线写方,结果不变;万一还有截断数据的写方在线,它不能凭旧令牌跳过 C3。
    redis.call('HDEL',KEYS[8],'schema_writer','schema_regressed')
elseif ARGV[4] ~= '0' then return redis.error_reply('restore mode invalid') end
return 1
`)
