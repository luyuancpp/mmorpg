package kafka

import (
	"context"
	"db/internal/metrics"
	"errors"
	"fmt"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	playerpb "proto/common/database"
	"shared/ownerepoch"
	"strconv"
	"sync"
	"sync/atomic"
	"time"

	db_config "db/internal/config"

	"github.com/luyuancpp/proto2mysql"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	dbpb "proto/db"
)

// fencingDual 为 true 时玩家两张表走行内围栏并发布资产 SQL 提交证据;false(缺省)= legacy,更新保留已有围栏但不拒写。
var fencingDual atomic.Bool
var fencingEpochOverride atomic.Bool

// SetFencingMode 设置行内围栏模式。**启动期设置一次、运行中不可改**:只在 db.go 启动、构造消费者之前调用;
// 运行中切换会让同一玩家的相邻两笔写分别走两种语义。测试里改它须用 t.Cleanup 复位成 legacy。
func SetFencingMode(mode string) error {
	for _, msg := range []proto.Message{&playerpb.PlayerDatabase{}, &playerpb.PlayerDatabase_1{}} {
		if !isFenceTable(msg.ProtoReflect().Descriptor()) {
			return fmt.Errorf("已知玩家表缺少正确围栏字段: %T", msg)
		}
		// 持久 schema 世代校验(persist_schema.go)按字段名反射读戳,字段缺失时没法判定,启动即拒。
		if err := requirePersistSchemaField(msg); err != nil {
			return err
		}
	}
	switch mode {
	case "", db_config.FencingModeLegacy:
		fencingDual.Store(false)
	case db_config.FencingModeDual:
		fencingDual.Store(true)
	default:
		return fmt.Errorf("unknown fencing mode %q (want %q or %q)", mode, db_config.FencingModeLegacy, db_config.FencingModeDual)
	}
	return nil
}

// writeOutcome 是 SQL 正常应答的业务结局；拒写不能冒充成功推进游标。
type writeOutcome struct {
	kind        string
	storedEpoch uint64
}

const (
	writeApplied        = "applied"
	writeEqual          = "equal"
	writeOlder          = "older_seq"
	writeStaleOwner     = "stale_owner"
	writeLegacyAfterSeq = "legacy_after_seq"
)

func (o writeOutcome) publishes() bool {
	return o.kind == writeApplied || o.kind == writeEqual
}

var fenceTableMetadata sync.Map

func isFenceTable(desc protoreflect.MessageDescriptor) bool {
	if cached, ok := fenceTableMetadata.Load(desc.FullName()); ok {
		return cached.(bool)
	}
	valid := true
	defer func() { fenceTableMetadata.Store(desc.FullName(), valid) }()
	for _, name := range []protoreflect.Name{"fence_owner_epoch", "fence_save_seq"} {
		f := desc.Fields().ByName(name)
		if f == nil || f.Kind() != protoreflect.Uint64Kind || f.IsList() || f.IsMap() || f.HasOptionalKeyword() {
			valid = false
			return false
		}
	}
	return true
}
func fenceClass(epoch, seq uint64) string {
	if epoch == 0 {
		return "legacy_zero"
	}
	if seq == 0 {
		return "legacy_epoch"
	}
	return "seq"
}

var errPlayerRowVanished = errors.New("玩家 SQL 行在探测与加锁之间消失")

// savePlayerRow 是 v0.2.0 过渡原语：已存在行锁定比较；新行直接 INSERT，撞唯一键后重读。
// legacy 更新也保留已有围栏；拒写不发布。equal 不比较内容，但只能从当前 SQL 行补发真实证据。
func savePlayerRow(ctx context.Context, store *proto2mysql.DB, task *dbpb.DBTask, msg proto.Message) (bool, writeOutcome, error) {
	desc := msg.ProtoReflect().Descriptor()
	if !isFenceTable(desc) {
		return false, writeOutcome{}, nil
	}
	id := desc.Fields().ByName("player_id")
	fo, fs := desc.Fields().ByName("fence_owner_epoch"), desc.Fields().ByName("fence_save_seq")
	if id == nil || id.Kind() != protoreflect.Uint64Kind || msg.ProtoReflect().Get(id).Uint() != task.GetKey() {
		return true, writeOutcome{}, errors.New("玩家 SQL 行身份与任务不符")
	}
	if task.SaveSeq != 0 && task.OwnerEpoch == 0 {
		return true, writeOutcome{}, errors.New("存盘序号缺少归属纪元")
	}
	class := fenceClass(task.OwnerEpoch, task.SaveSeq)
	if msg.ProtoReflect().Get(fo).Uint() != 0 || msg.ProtoReflect().Get(fs).Uint() != 0 {
		metrics.CountFenceBodyMismatch()
		logx.Errorf("FENCE-BODY-MISMATCH taskID=%s", task.TaskId)
	}
	epoch, seq := task.OwnerEpoch, task.SaveSeq
	if !fencingDual.Load() {
		epoch, seq = 0, 0
	}
	msg.ProtoReflect().Set(fo, protoreflect.ValueOfUint64(epoch))
	msg.ProtoReflect().Set(fs, protoreflect.ValueOfUint64(seq))
	target := proto.Clone(msg)
	probe := msg.ProtoReflect().Type().New().Interface()
	probe.ProtoReflect().Set(id, protoreflect.ValueOfUint64(task.Key))
	exists, err := store.WithContext(ctx).ExistsByPK(probe)
	if err != nil {
		metrics.CountFenceWrite(class, "error")
		return true, writeOutcome{}, err
	}
	if !exists {
		err = store.WithContext(ctx).Insert(target)
		if err == nil {
			metrics.CountFenceWrite(class, "inserted")
			return true, writeOutcome{kind: writeApplied}, nil
		}
		if !errors.Is(err, proto2mysql.ErrDuplicateKey) {
			metrics.CountFenceWrite(class, "error")
			return true, writeOutcome{}, err
		}
	}
	outcome := writeOutcome{kind: writeApplied}
	err = store.WithContext(ctx).RunInTransaction(func(tx *proto2mysql.DB) error {
		current := msg.ProtoReflect().Type().New().Interface()
		current.ProtoReflect().Set(id, protoreflect.ValueOfUint64(task.Key))
		if err := tx.FindOneByPKForUpdate(current); err != nil {
			if errors.Is(err, proto2mysql.ErrNoRowsFound) {
				return errPlayerRowVanished
			}
			return err
		}
		oldEpoch, oldSeq := current.ProtoReflect().Get(fo).Uint(), current.ProtoReflect().Get(fs).Uint()
		outcome.storedEpoch = oldEpoch
		if !fencingDual.Load() {
			// C1b(persist_schema.go):与写入同一把行锁。legacy 没有版本判定,倒退写同样不能盖掉更新世代写下的数据。
			if schemaRegressed(target, current) {
				outcome.kind = writeSchemaRegression
				return nil
			}
			target.ProtoReflect().Set(fo, protoreflect.ValueOfUint64(oldEpoch))
			target.ProtoReflect().Set(fs, protoreflect.ValueOfUint64(oldSeq))
			return tx.Save(target)
		}
		switch {
		case epoch == 0 && (oldEpoch != 0 || oldSeq != 0):
			outcome.kind = writeLegacyAfterSeq
		case epoch < oldEpoch:
			outcome.kind = writeStaleOwner
		case epoch != 0 && seq != 0 && epoch == oldEpoch && seq < oldSeq:
			outcome.kind = writeOlder
		case epoch != 0 && seq != 0 && epoch == oldEpoch && seq == oldSeq:
			outcome.kind = writeEqual
			// 首次 SQL 提交后若未发布证据就丢租，重放载荷也不能伪造已提交账本。
			proto.Reset(msg)
			proto.Merge(msg, current)
		default:
			// C1b:围栏判定之后、写入之前。围栏的 older / equal / stale 结局优先;到这里说明版本上允许写,
			// 但 body 的世代低于行世代 —— 链路上有人截断过数据,整行写会把新世代的字段盖掉(设计 §6.2)。
			if schemaRegressed(target, current) {
				outcome.kind = writeSchemaRegression
				return nil
			}
			if seq == 0 && epoch != 0 && oldEpoch == epoch {
				target.ProtoReflect().Set(fs, protoreflect.ValueOfUint64(oldSeq))
			}
			return tx.Save(target)
		}
		return nil
	})
	if err != nil {
		label := "error"
		if errors.Is(err, errPlayerRowVanished) {
			label = "race"
		}
		metrics.CountFenceWrite(class, label)
		return true, outcome, err
	}
	label := outcome.kind
	if label == writeApplied {
		label = "updated"
	}
	if outcome.kind != writeStaleOwner {
		metrics.CountFenceWrite(class, label)
	}
	if outcome.kind != writeStaleOwner && (!outcome.publishes() || outcome.kind == writeEqual) {
		metrics.CountFenceRejectedBytes(label, len(task.Body))
	}
	return true, outcome, nil
}

func SetFencingEpochOverride(enabled bool) { fencingEpochOverride.Store(enabled) }

// ownerCounterRegressed 只比较已落库epoch与已铸造计数器；缺键等于0，读失败不能假装未倒退。
func (w *worker) ownerCounterRegressed(playerID, stored uint64) (bool, error) {
	ctx, cancel := context.WithTimeout(context.WithoutCancel(w.ctx), orderingLockReleaseTimeout)
	defer cancel()
	start := time.Now()
	rc := w.ownerEpochRedis
	if rc == nil {
		rc = w.redisClient
	}
	raw, err := rc.Get(ctx, ownerepoch.OwnerEpochKey(playerID)).Result()
	metrics.ObserveRedisStep("write", metrics.RedisStepOwnerEpochProbe, time.Since(start))
	if errors.Is(err, redis.Nil) {
		if stored > 0 {
			logx.Errorf("OWNER-EPOCH-REGRESSED playerID=%d counterEpoch=0 committedEpoch=%d", playerID, stored)
		}
		return stored > 0, nil
	}
	if err != nil {
		return false, fmt.Errorf("%w: owner counter: %w", errFenceRedis, err)
	}
	epoch, err := ownerepoch.ParseEpoch(raw)
	if err != nil || strconv.FormatUint(epoch, 10) != raw {
		return false, fmt.Errorf("%w: owner counter is not canonical uint64", errFenceRedis)
	}
	if epoch < stored {
		logx.Errorf("OWNER-EPOCH-REGRESSED playerID=%d counterEpoch=%d committedEpoch=%d", playerID, epoch, stored)
	}
	return epoch < stored, nil
}
