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
    if redis.call('TYPE',KEYS[i]).ok ~= 'string' or redis.call('GET',KEYS[i]) ~= ARGV[i-67]
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
        if field ~= 'pending' and field ~= 'player_database' and field ~= 'player_database_1' and field ~= 'schema_gen' then
            return redis.error_reply('barrier or outbox conflicts')
        end
    end
    if redis.call('PTTL',KEYS[8]) ~= -1
        or redis.call('HGET',KEYS[8],'pending') ~= ARGV[3]
        or redis.call('HGET',KEYS[8],'player_database') ~= ARGV[3]
        or redis.call('HGET',KEYS[8],'player_database_1') ~= ARGV[3] then
        return redis.error_reply('barrier or outbox conflicts')
    end
    local stored=redis.call('HGET',KEYS[8],'schema_gen')
    if stored then
        if not is_schema_gen(stored) then return redis.error_reply('barrier schema generation invalid') end
        if #stored > #schema_gen or (#stored == #schema_gen and version_gt(stored,schema_gen)) then schema_gen=stored end
    end
end
for i=9,72 do
    local kind=redis.call('TYPE',KEYS[i]).ok
    if kind ~= 'none' and kind ~= 'zset' then return redis.error_reply('pending index wrong type') end
    if kind == 'zset' and redis.call('ZCARD',KEYS[i]) ~= 0 then return redis.error_reply('zone pending outbox not drained') end
end
if ARGV[4] == '1' then
    redis.call('HSET',KEYS[8],'pending',ARGV[3],'player_database',ARGV[3],'player_database_1',ARGV[3],'schema_gen',schema_gen)
elseif ARGV[4] ~= '0' then return redis.error_reply('restore mode invalid') end
return 1
`)
