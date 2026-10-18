package dataloader

import (
	"context"
	"fmt"
	componentpb "proto/common/component"
	databasepb "proto/common/database"
	"shared/ownerepoch"
	"shared/savebarrier"
	"strconv"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
)

// 父键、owner 和屏障必须位于同一 Redis 实例；既有父键/owner 命名不支持 Cluster 多键脚本。
// 读到的令牌只约束本次组装，发布时还要原子复核，不能把预检当提交点。
type parentLoadSnapshot struct {
	owner   string
	pending string
	raw     string
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

var beginParentLoadScript = redis.NewScript(parentBarrierLua + `
local state, err = inspect()
if err then return redis.error_reply(err) end
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

func beginParentLoad(ctx context.Context, rc redis.Cmdable, parentKey string, playerID uint64) (parentLoadSnapshot, error) {
	var snapshot parentLoadSnapshot
	values, err := beginParentLoadScript.Run(ctx, rc, parentBarrierKeys(parentKey, playerID)).StringSlice()
	if err != nil {
		return snapshot, fmt.Errorf("读取父快照提交屏障: %w", err)
	}
	if len(values) != 3 {
		return snapshot, fmt.Errorf("父快照屏障回复长度错误")
	}
	snapshot = parentLoadSnapshot{raw: values[0], owner: values[1], pending: values[2]}
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
	} else if snapshot.pending == "" && ownerEpoch != 0 {
		return snapshot, fmt.Errorf("历史玩家父快照与提交屏障缺失，需要停写恢复")
	}
	return snapshot, nil
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
