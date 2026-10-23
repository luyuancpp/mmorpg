package barrierrestore

import (
	"github.com/redis/go-redis/v9"
	"shared/savebarrier"
)

const luaHandoffFenceHelpers = `
local function bytecmp(a,b)
    if #a~=#b then if #a>#b then return 1 else return -1 end end
    for i=1,#a do
        local x,y=string.byte(a,i),string.byte(b,i)
        if x~=y then if x>y then return 1 else return -1 end end
    end
    return 0
end
local function uint(s, zero)
    if type(s)~='string' or #s==0 or #s>20 or not string.match(s,'^%d+$') then return false end
    if s=='0' then return zero==true end
    if string.sub(s,1,1)=='0' then return false end
    return bytecmp(s,'18446744073709551615')<=0
end
local function cmp(a,b)
    return bytecmp(a,b)
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
    for i,key in ipairs(KEYS) do
        if key=='' then return false end
        for j=1,i-1 do if key==KEYS[j] then return false end end
    end
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
        and bytecmp(string.sub(s,1,20),'18446744073709551615')<=0
        and bytecmp(string.sub(s,22),'18446744073709551615')<=0 and string.sub(s,22)~=string.rep('0',20)
end
local function marker(s,e)
    local owner,time=string.match(s or '','^(%d+):(%d+)$')
    return owner==e and uint(time)
end
`

// 所有检查先于第一次写。维护键只是本工具互斥及人工停写声明，业务写者不会自动读取它。
var restoreScript = redis.NewScript(savebarrier.LuaVersionHelpers + luaHandoffFenceHelpers + `
local function is_schema_gen(s)
    if type(s) ~= 'string' or #s == 0 or #s > 10 or not string.match(s,'^[1-9]%d*$') then return false end
    return #s < 10 or not version_gt(s,'4294967295')
end
if (#KEYS ~= 75 and #KEYS ~= 72) or #ARGV ~= 10 or not is_version(ARGV[3])
    or not is_schema_gen(ARGV[8]) or not nonce(ARGV[10]) then return redis.error_reply('restore arguments invalid') end
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
            and field ~= 'schema_writer' and field ~= 'schema_regressed' and field ~= 'handoff_fence' then
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
-- 即使原字段缺席也写永久 R 墓碑；恢复不是重新授权某个仍活的旧加载。
local prior=redis.call('HGET',KEYS[8],'handoff_fence')
local original=nil
if prior~=false then
    original=fence(prior)
    if not original or cmp(original.e,ARGV[2])>0 then return redis.error_reply('handoff floor invalid') end
end
if (prior or '')~=ARGV[9] then return redis.error_reply('handoff floor changed') end
local restored=ARGV[2]..'||0|'..string.rep('0',32)..'|R|'..ARGV[10]
if original then
    if original.t==ARGV[10] then return redis.error_reply('handoff token must rotate') end
    restored=original.e..'|'..original.i..'|'..original.n..'|'..original.a..'|R|'..ARGV[10]
end
if ARGV[4]~='0' and ARGV[4]~='1' then return redis.error_reply('restore mode invalid') end
for i=9,72 do
    local kind=redis.call('TYPE',KEYS[i]).ok
    if kind ~= 'none' and kind ~= 'zset' then return redis.error_reply('pending index wrong type') end
    if kind == 'zset' and redis.call('ZCARD',KEYS[i]) ~= 0 then return redis.error_reply('zone pending outbox not drained') end
end
if ARGV[4] == '1' then
    redis.call('HSET',KEYS[8],'pending',ARGV[3],'player_database',ARGV[3],'player_database_1',ARGV[3],'schema_gen',schema_gen,'handoff_fence',restored)
    -- 恢复以 SQL 为准:清掉写方令牌与 warn 倒退证据,下一次任何写方的严格屏障存盘都重新做一次世代比较(fail-closed)。
    -- 合规流程里恢复时没有在线写方,结果不变;万一还有截断数据的写方在线,它不能凭旧令牌跳过 C3。
    redis.call('HDEL',KEYS[8],'schema_writer','schema_regressed')
elseif ARGV[4] ~= '0' then return redis.error_reply('restore mode invalid') end
return 1
`)
