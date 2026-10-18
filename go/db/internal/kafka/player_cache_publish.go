package kafka

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"time"

	"db/internal/metrics"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	assetpb "proto/common/asset"
	dbpb "proto/db"
	"shared/assetop"
	"shared/savebarrier"
)

var errFenceEvidenceConflict = errors.New("同一 SQL 围栏版本的资产证据冲突")
var errFenceRedis = errors.New("围栏 Redis 步骤失败")

// 所有类型检查与冲突检查都先于写；Redis 脚本出错不会回滚已经写出的命令。
// KEYS: 锁、缓存、证据或缓存占位、存盘屏障或缓存占位。
// ARGV: 令牌、锁TTL毫秒、缓存原件、缓存TTL毫秒、版本、证据原件、发布证据、屏障表名。
const playerCachePublishLua = savebarrier.LuaVersionHelpers + `
if redis.call('GET',KEYS[1]) ~= ARGV[1] then return 0 end
if not tonumber(ARGV[2]) or tonumber(ARGV[2])<=0 or not tonumber(ARGV[4]) or tonumber(ARGV[4])<=0 then return redis.error_reply('invalid publish TTL') end
local writeEvidence=false
local result=1
if ARGV[7]=='1' then
 if not is_version(ARGV[5]) then return redis.error_reply('asset sql commit version is malformed') end
 local old=redis.call('HGET',KEYS[3],'version')
 if not old then writeEvidence=true
 elseif not is_version(old) then return redis.error_reply('stored asset sql commit version is malformed')
 elseif old==ARGV[5] then
  if redis.call('HGET',KEYS[3],'data')~=ARGV[6] then return -1 end
 elseif version_gt(ARGV[5],old) then writeEvidence=true
 else result=2 end
end
local writeBarrier=false
if ARGV[8]~='' then
 if not is_version(ARGV[5]) then return redis.error_reply('save barrier version is malformed') end
 local old=redis.call('HGET',KEYS[4],ARGV[8])
 if old and not is_version(old) then return redis.error_reply('stored save barrier version is malformed') end
 writeBarrier=not old or version_gt(ARGV[5],old)
end
redis.call('PEXPIRE',KEYS[1],ARGV[2])
redis.call('SET',KEYS[2],ARGV[3],'PX',ARGV[4])
if writeEvidence then redis.call('HSET',KEYS[3],'version',ARGV[5],'data',ARGV[6]) end
if writeBarrier then redis.call('HSET',KEYS[4],ARGV[8],ARGV[5]) end
return result
`

var playerCachePublishScript = redis.NewScript(playerCachePublishLua)

const appliedAdvanceLua = luaDecimalGreaterFn + `
if redis.call('GET',KEYS[1])~=ARGV[1] then return 0 end
local e=ARGV[4]
if not is_dec(e) or #e>20 or (#e==20 and dec_gt(e,'18446744073709551615')) then return redis.error_reply('task owner epoch is not canonical uint64') end
local cur=false
if e~='0' then
 cur=redis.call('GET',KEYS[3])
 if cur and (not is_dec(cur) or #cur>20 or (#cur==20 and dec_gt(cur,'18446744073709551615'))) then return redis.error_reply('stored owner epoch is not canonical uint64') end
end
if not tonumber(ARGV[2]) or tonumber(ARGV[2])<=0 then return redis.error_reply('invalid applied TTL') end
redis.call('PEXPIRE',KEYS[1],ARGV[2])
if ARGV[3]~='' then redis.call('SET',KEYS[2],ARGV[3]) end
if e~='0' and (not cur or dec_gt(e,cur)) then redis.call('SET',KEYS[3],e) end
return 1
`

var appliedAdvanceScript = redis.NewScript(appliedAdvanceLua)

func publishDBWriteResult(ctx context.Context, lease writeLease, task *dbpb.DBTask, msg proto.Message) error {
	raw, err := proto.Marshal(msg)
	if err != nil {
		return err
	}
	cache := buildCacheKey(task)
	keys := []string{cache, cache, cache}
	version, evidence, hasEvidence, barrierField := "", []byte(nil), "0", ""
	if fencingDual.Load() && task.OwnerEpoch != 0 && task.SaveSeq != 0 && isFenceTable(msg.ProtoReflect().Descriptor()) {
		version = savebarrier.Version(task.OwnerEpoch, task.SaveSeq)
		barrierField = task.MsgType
		keys[2] = savebarrier.Key(task.Key)
		if task.MsgType == "player_database" {
			field := msg.ProtoReflect().Descriptor().Fields().ByName("asset_op_ledger")
			ledger, marshalErr := (proto.MarshalOptions{Deterministic: true}).Marshal(msg.ProtoReflect().Get(field).Message().Interface())
			if marshalErr != nil {
				return marshalErr
			}
			evidence, err = (proto.MarshalOptions{Deterministic: true}).Marshal(&assetpb.PlayerAssetSQLCommit{PlayerId: task.Key, OwnerEpoch: task.OwnerEpoch, SaveSeq: task.SaveSeq, LedgerProto: ledger})
			if err != nil {
				return err
			}
			keys[1] = assetop.SQLCommitKey(task.Key)
			hasEvidence = "1"
		}
	}
	start := time.Now()
	result, err := lease.runTokenScript(ctx, playerCachePublishScript, keys, raw, cacheTTL().Milliseconds(), version, evidence, hasEvidence, barrierField)
	cost := time.Since(start)
	metrics.ObserveStage(metrics.StageCacheWrite, "write", cost)
	metrics.ObserveRedisStep("write", metrics.RedisStepCachePublish, cost)
	if err != nil {
		return err
	}
	switch result {
	case 0:
		metrics.CountFenceCache("write", "not_owner")
		return fmt.Errorf("%w: cache publisher no longer owns lock", errOrderingLeaseLost)
	case -1:
		metrics.CountFenceCache("write", "evidence_conflict")
		logx.Errorf("ASSET-SQL-COMMIT-CONFLICT taskID=%s version=%s", task.TaskId, version)
		return errFenceEvidenceConflict
	case 1:
		metrics.CountFenceCache("write", "written")
	case 2:
		metrics.CountFenceCache("write", "written")
		metrics.CountFenceCache("write", "evidence_ahead")
		logx.Errorf("ASSET-SQL-COMMIT-AHEAD taskID=%s version=%s", task.TaskId, version)
	default:
		return fmt.Errorf("unexpected cache publication result %d", result)
	}
	return nil
}

func advanceApplied(ctx context.Context, lease writeLease, topic string, dbTask *dbpb.DBTask, task *workerTask) error {
	if dbTask.Op != "write" {
		return nil
	}
	cursor := ""
	if task.seq != 0 {
		cursor = encodeAppliedCursor(appliedCursor{seq: task.seq, partition: task.originPartition, hasPartition: task.hasOriginPartition})
	}
	start := time.Now()
	result, err := lease.runTokenScript(ctx, appliedAdvanceScript, []string{appliedSeqKey(topic, dbTask.Key, dbTask.MsgType), appliedEpochKey(topic, dbTask.Key, dbTask.MsgType)}, cursor, strconv.FormatUint(dbTask.OwnerEpoch, 10))
	metrics.ObserveRedisStep("write", metrics.RedisStepAppliedAdvance, time.Since(start))
	if err != nil {
		return err
	}
	if result == 0 {
		return fmt.Errorf("%w: applied publisher no longer owns lock", errOrderingLeaseLost)
	}
	return nil
}
