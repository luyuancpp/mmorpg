package dataloader

import (
	"context"
	"fmt"
	"login/internal/config"
	componentpb "proto/common/component"
	databasepb "proto/common/database"
	"shared/ownerepoch"
	"shared/savebarrier"
	"strconv"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
)

// 父键、owner 和屏障必须位于同一 Redis 实例；既有父键/owner 命名不支持 Cluster 多键脚本。
// 读到的令牌只约束本次组装，发布时还要原子复核，不能把预检当提交点。
type parentLoadSnapshot struct {
	owner   string
	pending string
	raw     string
	// schemaGen 是屏障 hash 里 scene 记录的持久 schema 世代原文(C4 用);空串 = hash 不存在或没有该字段。
	// 只是索引不是权威(设计 §3.3),publish 不复核它。
	schemaGen string
}

const parentBarrierLua = savebarrier.LuaVersionHelpers + `
local function key_type(key, expected)
    local t = redis.call('TYPE', key).ok
    return t == 'none' or t == expected
end
local function inspect()
    if not key_type(KEYS[1], 'string') or not key_type(KEYS[2], 'string')
        or not key_type(KEYS[3], 'hash') then
        return nil, 'parent load key type mismatch'
    end
    local p = redis.call('HGET', KEYS[3], 'pending') or ''
    local a = redis.call('HGET', KEYS[3], 'player_database') or ''
    local b = redis.call('HGET', KEYS[3], 'player_database_1') or ''
    if p ~= '' then
        if not is_version(p) or not is_version(a) or not is_version(b) then
            return nil, 'parent SQL barrier missing or malformed'
        end
        if a ~= p or b ~= p then return nil, 'parent SQL barrier not committed' end
    elseif a ~= '' or b ~= '' then
        return nil, 'parent pending barrier missing'
    end
    return {redis.call('GET', KEYS[1]) or '', redis.call('GET', KEYS[2]) or '', p}, nil
end
`

// 在 inspect 的三元组后追加第 4 个元素:屏障 hash 的 schema_gen(不存在时为空串),供 C4 比较。
// 只在这里追加、不改共享的 inspect:publish 只读前 3 个元素,不受影响。
var beginParentLoadScript = redis.NewScript(parentBarrierLua + `
local state, err = inspect()
if err then return redis.error_reply(err) end
state[4] = redis.call('HGET', KEYS[3], 'schema_gen') or ''
return state
`)

var publishParentLoadScript = redis.NewScript(parentBarrierLua + `
local state, err = inspect()
if err then return redis.error_reply(err) end
if state[2] ~= ARGV[1] or state[3] ~= ARGV[2] then
    return redis.error_reply('parent ownership or pending save changed during SQL load')
end
if state[1] ~= '' then return 0 end
redis.call('SET', KEYS[1], ARGV[3], 'PX', ARGV[4], 'NX')
return 1
`)

func parentBarrierKeys(parentKey string, playerID uint64) []string {
	return []string{parentKey, ownerepoch.OwnerEpochKey(playerID), savebarrier.Key(playerID)}
}

// beginParentLoad 读父快照与提交屏障并做全部只读校验。regressionMode 必须是
// config.ParsePersistSchemaRegressionMode 归一化之后的值,只决定 C4 判定倒退时放行还是返回错误。
func beginParentLoad(ctx context.Context, rc redis.Cmdable, parentKey string, playerID uint64,
	regressionMode config.PersistSchemaRegressionMode) (parentLoadSnapshot, error) {
	var snapshot parentLoadSnapshot
	values, err := beginParentLoadScript.Run(ctx, rc, parentBarrierKeys(parentKey, playerID)).StringSlice()
	if err != nil {
		return snapshot, fmt.Errorf("读取父快照提交屏障: %w", err)
	}
	if len(values) != 4 {
		return snapshot, fmt.Errorf("父快照屏障回复长度错误")
	}
	snapshot = parentLoadSnapshot{raw: values[0], owner: values[1], pending: values[2], schemaGen: values[3]}
	ownerEpoch, err := ownerepoch.ParseEpoch(snapshot.owner)
	if err != nil || (snapshot.owner != "" && strconv.FormatUint(ownerEpoch, 10) != snapshot.owner) {
		return snapshot, fmt.Errorf("父快照归属计数器不规范")
	}
	if snapshot.pending != "" {
		epoch, seq, err := savebarrier.Parse(snapshot.pending)
		if err != nil || epoch == 0 || seq == 0 || ownerEpoch < epoch {
			return snapshot, fmt.Errorf("父快照归属计数器缺失或落后于提交屏障")
		}
	}
	if snapshot.raw != "" {
		parent := new(databasepb.PlayerAllData)
		if err := proto.Unmarshal([]byte(snapshot.raw), parent); err != nil {
			return snapshot, fmt.Errorf("父快照解析失败: %w", err)
		}
		if parent.GetPlayerDatabaseData().GetPlayerId() != playerID || parent.GetPlayerDatabase_1Data().GetPlayerId() != playerID {
			return snapshot, fmt.Errorf("父快照玩家身份不符")
		}
		if snapshot.pending == "" {
			// 首次身份补齐与铸造owner之间可断线；只允许身份字段，不猜测任何历史资产的提交上界。
			if ownerEpoch != 0 || !isUnownedLoginSeed(parent, playerID) {
				return snapshot, fmt.Errorf("历史父快照缺少提交屏障，需要停写恢复或迁移")
			}
		} else {
			_, seq, _ := savebarrier.Parse(snapshot.pending)
			if parent.GetLastSaveSeq() != seq {
				return snapshot, fmt.Errorf("父快照水位与提交屏障不符")
			}
		}
		// C4 放在身份、水位、种子校验之后,只判快路径:冷加载要求两表 SQL ACK 等于 pending,
		// 行戳必然 ≥ 屏障记录的世代(设计 §5.3、§5.4),不需要也不应该在这里判。
		if err := checkParentSchemaRegression(parent, snapshot.schemaGen, playerID, regressionMode); err != nil {
			return snapshot, err
		}
	} else if snapshot.pending == "" && ownerEpoch != 0 {
		return snapshot, fmt.Errorf("历史玩家父快照与提交屏障缺失，需要停写恢复")
	}
	return snapshot, nil
}

// checkParentSchemaRegression 是 C4(设计 §5.1):父快照的加载世代低于屏障记录的 schema_gen,
// 或 schema_gen 存在但不是规范的十进制 uint32,都算倒退证据(数据可能已被不认识新字段的旧写方截断)。
// 屏障里没有 schema_gen(机制前的存量玩家、merge_zone 删过 hash、新号种子)一律不比较(设计 §3.3)。
// 两种模式都计数并打日志;只有 enforce 返回错误,走 EnterGame 现有失败路径(固定 kEnterSceneFailed,
// 原因不外发给客户端,不新增 tip)。
func checkParentSchemaRegression(parent *databasepb.PlayerAllData, barrierGen string, playerID uint64,
	mode config.PersistSchemaRegressionMode) error {
	if barrierGen == "" {
		return nil
	}
	loaded := loadedSchemaGen(parent)
	recorded, ok := parseBarrierSchemaGen(barrierGen)
	var reason string
	switch {
	case !ok:
		reason = schemaRegressionReasonMalformed
	case loaded < recorded:
		reason = schemaRegressionReasonRegression
	default:
		return nil
	}
	enforce := mode == config.PersistSchemaRegressionEnforce
	// label 只取两个字面量:调用方传入的是归一化后的模式,这里再收口一次,保证序列集合封闭。
	modeLabel, action := string(config.PersistSchemaRegressionWarn), "admitted"
	if enforce {
		modeLabel, action = string(config.PersistSchemaRegressionEnforce), "rejected"
	}
	persistSchemaRegressionTotal.Inc(reason, modeLabel)
	// go-zero logx 没有 WARN 级别,沿用本服务 "WARNING" 前缀 + Errorf 的写法。
	// barrierGen 可能是损坏的任意字节,%.32q 截断并转义,不让脏值撑爆日志行。
	logx.Errorf("WARNING persist schema regression on parent snapshot fast path: playerId=%d reason=%s "+
		"loadedGen=%d barrierGen=%.32q mode=%s action=%s", playerID, reason, loaded, barrierGen, modeLabel, action)
	if !enforce {
		return nil
	}
	return fmt.Errorf("父快照持久 schema 世代倒退(%s):加载世代 %d,屏障记录 %.32q", reason, loaded, barrierGen)
}

// loadedSchemaGen 是父快照的加载世代:两张表戳取最小值,未盖戳的 0 按 1(设计 §3.3)。
// 取最小值是为了不让一张表被截断的事实被另一张表的高戳洗白。
func loadedSchemaGen(parent *databasepb.PlayerAllData) uint32 {
	return min(effectiveSchemaGen(parent.GetPlayerDatabaseData().GetPersistSchemaGen()),
		effectiveSchemaGen(parent.GetPlayerDatabase_1Data().GetPersistSchemaGen()))
}

func effectiveSchemaGen(raw uint32) uint32 {
	if raw == 0 {
		return 1
	}
	return raw
}

// parseBarrierSchemaGen 只认 scene 写入的规范形式:十进制、>= 1、无符号、无前导零、不超过 uint32。
// 与 go/db save_barrier_restore 脚本的 is_schema_gen 同一口径;不规范即视为损坏。
func parseBarrierSchemaGen(raw string) (uint32, bool) {
	v, err := strconv.ParseUint(raw, 10, 32)
	if err != nil || v == 0 || strconv.FormatUint(v, 10) != raw {
		return 0, false
	}
	return uint32(v), true
}

// backfillPlayerIdentity在首次owner之前只补这四个身份字段。清除白名单后必须逐字段等于
// 建角种子，未知字段也参与比较；不得把注册记录、货币或其他旧档数据当成可重建的新号。
// clone只用于只读校验，不能把清除后的副本回写覆盖真实身份。
func isUnownedLoginSeed(parent *databasepb.PlayerAllData, playerID uint64) bool {
	if parent == nil {
		return false
	}
	seed := proto.Clone(parent).(*databasepb.PlayerAllData)
	row := seed.GetPlayerDatabaseData()
	if row == nil {
		return false
	}
	if class := row.GetUint32PbComponent(); class != nil {
		class.Class = 0
		if proto.Equal(class, &componentpb.PlayerUint32Comp{}) {
			row.Uint32PbComponent = nil
		}
	}
	if profile := row.GetProfileComponent(); profile != nil {
		profile.Name, profile.AppearanceId, profile.Gender = "", "", 0
		if proto.Equal(profile, &componentpb.PlayerProfileComp{}) {
			row.ProfileComponent = nil
		}
	}
	return proto.Equal(seed, newPlayerAllDataSkeleton(playerID))
}

// validateSQLSub 只接受真正的 SQL 回包；冷恢复不采信可能落后一代的 Redis 子表缓存。
func (snapshot parentLoadSnapshot) validateSQLSub(sub proto.Message, playerID uint64) error {
	row, ok := sub.(interface {
		GetPlayerId() uint64
		GetFenceOwnerEpoch() uint64
		GetFenceSaveSeq() uint64
	})
	if !ok || row.GetPlayerId() != playerID {
		return fmt.Errorf("冷恢复 SQL 子表身份或类型不符")
	}
	if snapshot.pending == "" {
		if row.GetFenceOwnerEpoch() != 0 || row.GetFenceSaveSeq() != 0 {
			return fmt.Errorf("SQL 已有历史围栏但 Redis 归属历史缺失，需要停写恢复")
		}
		empty := sub.ProtoReflect().Type().New()
		empty.Set(empty.Descriptor().Fields().ByName("player_id"), protoreflect.ValueOfUint64(playerID))
		if !proto.Equal(sub, empty.Interface()) {
			return fmt.Errorf("SQL 存在历史数据但缺少提交屏障，需要停写迁移")
		}
		return nil
	}
	if savebarrier.Version(row.GetFenceOwnerEpoch(), row.GetFenceSaveSeq()) != snapshot.pending {
		return fmt.Errorf("SQL 子表围栏尚未对齐父快照提交屏障")
	}
	return nil
}

func (snapshot parentLoadSnapshot) publish(ctx context.Context, rc redis.Cmdable, parentKey string, playerID uint64, parent *databasepb.PlayerAllData) error {
	if snapshot.pending != "" {
		_, seq, err := savebarrier.Parse(snapshot.pending)
		if err != nil {
			return err
		}
		parent.LastSaveSeq = seq
	}
	data, err := proto.Marshal(parent)
	if err != nil {
		return err
	}
	_, err = publishParentLoadScript.Run(ctx, rc, parentBarrierKeys(parentKey, playerID), snapshot.owner, snapshot.pending, data, playerAllDataParentTTL.Milliseconds()).Int()
	return err
}
