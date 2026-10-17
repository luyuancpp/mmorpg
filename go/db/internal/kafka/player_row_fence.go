package kafka

import (
	"context"
	"errors"
	"fmt"
	"sync/atomic"

	db_config "db/internal/config"

	"github.com/luyuancpp/proto2mysql"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoreflect"
	dbpb "proto/db"
)

// fencingDual 为 true 时玩家两张表走行内围栏并发布资产 SQL 提交证据;false(缺省)= legacy,照旧普通 Save。
var fencingDual atomic.Bool

// SetFencingMode 设置行内围栏模式。**启动期设置一次、运行中不可改**:只在 db.go 启动、构造消费者之前调用;
// 运行中切换会让同一玩家的相邻两笔写分别走两种语义。测试里改它须用 t.Cleanup 复位成 legacy。
func SetFencingMode(mode string) error {
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

// 旧归属/旧存盘已被 SQL 拒绝。这是终局过期写，不能再发布它携带的旧缓存。
var errPlayerRowStale = errors.New("玩家 SQL 行版本已更新")

// 行在非锁定探测与锁定读之间消失。可重试：下一轮重新探测后走插入分支。
var errPlayerRowVanished = errors.New("玩家 SQL 行在探测与加锁之间消失")

// savePlayerRow 使用现有 proto2mysql 的插入与主键行锁事务，不引入第二套 SQL 映射器。
// 行不存在：自动提交地 INSERT；行存在：事务内 FOR UPDATE 命中记录锁（无间隙锁）后按版本裁决再 Save。
// 返回 handled=false 时调用者沿用普通表 Save。玩家行一旦收到带版本的写，旧生产者不能覆盖它。
func savePlayerRow(ctx context.Context, store *proto2mysql.DB, task *dbpb.DBTask, msg proto.Message) (bool, error) {
	desc := msg.ProtoReflect().Descriptor()
	if desc.FullName() != "player_database" && desc.FullName() != "player_database_1" {
		return false, nil
	}
	if !fencingDual.Load() {
		// legacy:不判围栏,调用者照旧普通 Save(HEAD 语义)。这是 C++ 回滚到旧版时不静默丢盘的保障(设计 §8)。
		return false, nil
	}
	idField := desc.Fields().ByName("player_id")
	epochField := desc.Fields().ByName("fence_owner_epoch")
	seqField := desc.Fields().ByName("fence_save_seq")
	if idField == nil || epochField == nil || seqField == nil {
		return true, fmt.Errorf("玩家 SQL 围栏字段缺失")
	}
	if msg.ProtoReflect().Get(idField).Uint() != task.GetKey() {
		return true, fmt.Errorf("玩家 SQL 行身份与任务不符")
	}
	epoch, seq := task.GetOwnerEpoch(), task.GetSaveSeq()
	versioned := epoch != 0 && seq != 0
	if seq != 0 && epoch == 0 {
		return true, fmt.Errorf("存盘序号缺少归属纪元")
	}
	target := proto.Clone(msg)
	// 版本只认生产信封，不接受载荷自行声称另一版本。旧生产者按未版本化写处理。
	if !versioned {
		epoch, seq = 0, 0
	}
	target.ProtoReflect().Set(epochField, protoreflect.ValueOfUint64(epoch))
	target.ProtoReflect().Set(seqField, protoreflect.ValueOfUint64(seq))

	// 行不存在时不能在锁定读事务里插入：go/db 跑在服务器全局 RR 上，对不存在主键的 FOR UPDATE 会取间隙锁，
	// 两个不同新号并发首存落在同一间隙时互相挡住插入意向锁而 1213，锁集越出「本玩家」（2026-10-16 真库复现，
	// 见 docs/ops/incident-friend-lock-order-deadlock-2026-09-21.md §9.6.3）。所以先非锁定地判断行在不在，
	// 不在就自动提交地直接 INSERT；不同主键的插入意向锁互相兼容。
	probe := msg.ProtoReflect().Type().New().Interface()
	probe.ProtoReflect().Set(idField, protoreflect.ValueOfUint64(task.GetKey()))
	exists, err := store.WithContext(ctx).ExistsByPK(probe)
	if err != nil {
		return true, err
	}
	if !exists {
		err = store.WithContext(ctx).Insert(target)
		if err == nil {
			proto.Reset(msg)
			proto.Merge(msg, target)
			return true, nil
		}
		if !errors.Is(err, proto2mysql.ErrDuplicateKey) {
			return true, err
		}
		// 撞 1062 只可能是同一玩家的另一笔首写已提交：落到下面的锁定比较，按版本裁决。
		// 不能用 Save 的「撞 1062 后按主键 UPDATE」兜底，那会绕过围栏、用旧版本覆盖新版本。
	}

	stale := false
	err = store.WithContext(ctx).RunInTransaction(func(tx *proto2mysql.DB) error {
		current := msg.ProtoReflect().Type().New().Interface()
		current.ProtoReflect().Set(idField, protoreflect.ValueOfUint64(task.GetKey()))
		err := tx.FindOneByPKForUpdate(current)
		if errors.Is(err, proto2mysql.ErrNoRowsFound) {
			// 行在探测与加锁之间被删（只有运维工具会删玩家行）：此时已持间隙锁，不在这个事务里插入，交给重试重新探测。
			return errPlayerRowVanished
		}
		if err != nil {
			return err
		}
		oldEpoch := current.ProtoReflect().Get(epochField).Uint()
		oldSeq := current.ProtoReflect().Get(seqField).Uint()
		if epoch < oldEpoch || epoch == oldEpoch && seq < oldSeq {
			// 原任务不能写，但发布阶段须使用实际 SQL 行，避免迟到 offset 越过此前失败的缓存补发。
			proto.Reset(target)
			proto.Merge(target, current)
			stale = true
			return nil
		}
		if versioned && epoch == oldEpoch && seq == oldSeq {
			// 同 (epoch, seq) 按幂等生产者口径视为重放，不比内容（db-write-fencing-mysql.md v0.4 §14 第 12 条）：
			// proto2mysql 把置位的空子消息写成空串、读回为未置位，而 C++ 总是置位全部子消息，按内容比对会把正常重放
			// 误判成冲突。发布阶段用 SQL 当前行，缓存与资产证据跟 MySQL 一致（先提交者为准）。跨进程撞号的根治是
			// v0.4 §7.2 的 C++ 进程级发号器（墙钟下限），它落码之前撞号时以 SQL 行为准。
			proto.Reset(target)
			proto.Merge(target, current)
			return nil
		}
		return tx.Save(target)
	})
	if err == nil {
		proto.Reset(msg)
		proto.Merge(msg, target)
		if stale {
			return true, errPlayerRowStale
		}
	}
	return true, err
}
