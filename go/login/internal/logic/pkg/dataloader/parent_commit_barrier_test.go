package dataloader

import (
	"context"
	"fmt"
	"login/internal/dispatcher"
	componentpb "proto/common/component"
	databasepb "proto/common/database"
	dbpb "proto/db"
	"shared/ownerepoch"
	"shared/savebarrier"
	"testing"
	"time"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/reflect/protoregistry"
)

// 替身只提供 SQL 读回包，组装、校验、CAS 以及完成回调均走正式入口。
type coldSQLReads struct {
	callbacks map[string]dispatcher.ResultCallback
	read      func(*dbpb.DBTask) ([]byte, error)
	count     int
}

func (reads *coldSQLReads) Register(id string, _ time.Duration, callback dispatcher.ResultCallback) {
	if reads.callbacks == nil {
		reads.callbacks = make(map[string]dispatcher.ResultCallback)
	}
	reads.callbacks[id] = callback
}

// ReplyInstance 满足 readResultRegistrar;替身不经 Pub/Sub 投递,给一个合法的固定值即可。
func (reads *coldSQLReads) ReplyInstance() string { return "cold-sql-reads" }

// Cancel 满足 readResultRegistrar;替身的 SendTasks 不返回错误,正常不会走到这里。
func (reads *coldSQLReads) Cancel(ids ...string) {
	for _, id := range ids {
		delete(reads.callbacks, id)
	}
}

func (reads *coldSQLReads) SendTask(ctx context.Context, task *dbpb.DBTask, key string) error {
	return reads.SendTasks(ctx, []*dbpb.DBTask{task}, key)
}

func (reads *coldSQLReads) SendTasks(_ context.Context, tasks []*dbpb.DBTask, _ string) error {
	for _, task := range tasks {
		reads.count++
		data, err := reads.read(task)
		reads.callbacks[task.TaskId](&dbpb.TaskResult{Success: err == nil, Data: data, Error: fmt.Sprint(err)}, nil)
	}
	return nil
}

func sqlReadAtVersion(epoch, seq uint64) func(*dbpb.DBTask) ([]byte, error) {
	return func(task *dbpb.DBTask) ([]byte, error) {
		typ, err := protoregistry.GlobalTypes.FindMessageByURL(task.MsgType)
		if err != nil {
			return nil, err
		}
		sub := typ.New().Interface()
		if err := proto.Unmarshal(task.Body, sub); err != nil {
			return nil, err
		}
		switch row := sub.(type) {
		case *databasepb.PlayerDatabase:
			row.FenceOwnerEpoch, row.FenceSaveSeq = epoch, seq
		case *databasepb.PlayerDatabase_1:
			row.FenceOwnerEpoch, row.FenceSaveSeq = epoch, seq
		}
		return proto.Marshal(sub)
	}
}

func installCommittedBarrier(t *testing.T, rc *redis.Client, epoch, seq uint64) {
	t.Helper()
	ctx := context.Background()
	if err := rc.Set(ctx, ownerepoch.OwnerEpochKey(seedTestPlayerID), epoch, 0).Err(); err != nil {
		t.Fatal(err)
	}
	v := savebarrier.Version(epoch, seq)
	if err := rc.HSet(ctx, savebarrier.Key(seedTestPlayerID), "pending", v, "player_database", v, "player_database_1", v).Err(); err != nil {
		t.Fatal(err)
	}
}

func TestColdRecoveryRequiresBothSQLReceipts(t *testing.T) {
	for _, field := range []string{"player_database", "player_database_1"} {
		t.Run(field, func(t *testing.T) {
			mr, rc := newSeedTestRedis(t)
			installCommittedBarrier(t, rc, 2, 99)
			if err := rc.HSet(context.Background(), savebarrier.Key(seedTestPlayerID), field, savebarrier.Version(2, 98)).Err(); err != nil {
				t.Fatal(err)
			}
			calls := 0
			EnsurePlayerAllDataInRedisAsync(context.Background(), rc, nil, nil, seedTestPlayerID, 0, func(err error) {
				calls++
				if err == nil {
					t.Error("未提交的子表被放行")
				}
			})
			if calls != 1 || mr.Exists(seedParentKey(seedTestPlayerID)) {
				t.Fatal("未提交恢复写入了父键")
			}
		})
	}
}

func TestColdRecoveryIgnoresSubCachesAndCarriesSQLFloor(t *testing.T) {
	_, rc := newSeedTestRedis(t)
	installCommittedBarrier(t, rc, 2, 99)
	for _, sub := range seedSubMessages(seedTestPlayerID) {
		raw, _ := proto.Marshal(sub)
		if err := rc.Set(context.Background(), seedSubKey(sub, seedTestPlayerID), raw, 0).Err(); err != nil {
			t.Fatal(err)
		}
	}
	reads := &coldSQLReads{read: sqlReadAtVersion(2, 99)}
	calls := 0
	EnsurePlayerAllDataInRedisAsync(context.Background(), rc, reads, reads, seedTestPlayerID, 0, func(err error) {
		calls++
		if err != nil {
			t.Error(err)
		}
	})
	parent, _ := readSeedParent(t, rc, seedTestPlayerID)
	if calls != 1 || reads.count != 2 || parent.LastSaveSeq != 99 || parent.GetPlayerDatabaseData().GetFenceSaveSeq() != 99 {
		t.Fatalf("恢复未沿 SQL 权威/播种水位: calls=%d reads=%d parent=%v", calls, reads.count, parent)
	}
}

func TestColdRecoveryRejectsOlderSQLAndMissingOwnershipHistory(t *testing.T) {
	for _, test := range []struct {
		name       string
		install    bool
		epoch, seq uint64
	}{
		{"SQL较旧", true, 2, 98}, {"SQL已换主", true, 3, 100}, {"Redis归属历史丢失", false, 2, 99},
	} {
		t.Run(test.name, func(t *testing.T) {
			mr, rc := newSeedTestRedis(t)
			if test.install {
				installCommittedBarrier(t, rc, 2, 99)
			}
			reads := &coldSQLReads{read: sqlReadAtVersion(test.epoch, test.seq)}
			calls := 0
			EnsurePlayerAllDataInRedisAsync(context.Background(), rc, reads, reads, seedTestPlayerID, 0, func(err error) {
				calls++
				if err == nil {
					t.Error("不可信 SQL 结果被放行")
				}
			})
			if calls != 1 || mr.Exists(seedParentKey(seedTestPlayerID)) {
				t.Fatal("拒绝恢复后出现父键")
			}
		})
	}
}

func TestColdRecoveryCASPreservesNewOwnerOrNewerSnapshot(t *testing.T) {
	for _, mutation := range []string{"owner", "pending", "parent"} {
		t.Run(mutation, func(t *testing.T) {
			_, rc := newSeedTestRedis(t)
			installCommittedBarrier(t, rc, 2, 99)
			ctx := context.Background()
			key := seedParentKey(seedTestPlayerID)
			snapshot, err := beginParentLoad(ctx, rc, key, seedTestPlayerID)
			if err != nil {
				t.Fatal(err)
			}
			switch mutation {
			case "owner":
				err = rc.Set(ctx, ownerepoch.OwnerEpochKey(seedTestPlayerID), "3", 0).Err()
			case "pending":
				err = rc.HSet(ctx, savebarrier.Key(seedTestPlayerID), "pending", savebarrier.Version(2, 100)).Err()
			case "parent":
				err = rc.Set(ctx, key, "newer-parent", 0).Err()
			}
			if err != nil {
				t.Fatal(err)
			}
			err = snapshot.publish(ctx, rc, key, seedTestPlayerID, newPlayerAllDataSkeleton(seedTestPlayerID))
			if mutation == "parent" {
				if err != nil || rc.Get(ctx, key).Val() != "newer-parent" {
					t.Fatal("覆盖了并发新父键", err)
				}
			} else if err == nil || rc.Exists(ctx, key).Val() != 0 {
				t.Fatal("令牌变化未阻止恢复")
			}
		})
	}
}

func TestColdRecoveryFailsClosedForMalformedBarrierAndRedisFailure(t *testing.T) {
	for _, fault := range []string{"wrong-type", "overflow", "missing-pending", "redis-error"} {
		t.Run(fault, func(t *testing.T) {
			mr, rc := newSeedTestRedis(t)
			installCommittedBarrier(t, rc, 2, 99)
			ctx := context.Background()
			switch fault {
			case "wrong-type":
				mr.Del(savebarrier.Key(seedTestPlayerID))
				mr.Set(savebarrier.Key(seedTestPlayerID), "corrupt")
			case "overflow":
				_ = rc.HSet(ctx, savebarrier.Key(seedTestPlayerID), "pending", "18446744073709551616:00000000000000000099").Err()
			case "missing-pending":
				_ = rc.HDel(ctx, savebarrier.Key(seedTestPlayerID), "pending").Err()
			case "redis-error":
				mr.SetError("injected Redis unavailable")
			}
			if _, err := beginParentLoad(ctx, rc, seedParentKey(seedTestPlayerID), seedTestPlayerID); err == nil {
				t.Fatal("损坏/不可用屏障未拒绝")
			}
		})
	}
}

func TestUnversionedHistoryCannotBeMistakenForNewPlayer(t *testing.T) {
	t.Run("种子已分配过归属", func(t *testing.T) {
		_, rc := newSeedTestRedis(t)
		ctx := context.Background()
		if _, err := SeedNewPlayerAllData(ctx, rc, seedTestPlayerID); err != nil {
			t.Fatal(err)
		}
		if err := rc.Set(ctx, ownerepoch.OwnerEpochKey(seedTestPlayerID), "1", 0).Err(); err != nil {
			t.Fatal(err)
		}
		if _, err := beginParentLoad(ctx, rc, seedParentKey(seedTestPlayerID), seedTestPlayerID); err == nil {
			t.Fatal("已分配归属的旧种子被当成新号")
		}
	})
	t.Run("历史零围栏资产", func(t *testing.T) {
		mr, rc := newSeedTestRedis(t)
		read := sqlReadAtVersion(0, 0)
		reads := &coldSQLReads{read: func(task *dbpb.DBTask) ([]byte, error) {
			raw, err := read(task)
			if task.MsgType != string((&databasepb.PlayerDatabase{}).ProtoReflect().Descriptor().FullName()) {
				return raw, err
			}
			row := new(databasepb.PlayerDatabase)
			if err := proto.Unmarshal(raw, row); err != nil {
				return nil, err
			}
			row.LevelComponent = &componentpb.LevelComp{Level: 30}
			return proto.Marshal(row)
		}}
		calls := 0
		EnsurePlayerAllDataInRedisAsync(context.Background(), rc, reads, reads, seedTestPlayerID, 0, func(err error) {
			calls++
			if err == nil {
				t.Error("零围栏历史资产被当成新号")
			}
		})
		if calls != 1 || mr.Exists(seedParentKey(seedTestPlayerID)) {
			t.Fatal("无屏障历史资产写成了父键")
		}
	})
}

func TestUnownedIdentitySeedDoesNotAdmitAssetsOrUnknownFields(t *testing.T) {
	unknown := protowire.AppendVarint(protowire.AppendTag(nil, 1000000, protowire.VarintType), 1)
	for _, tc := range []struct {
		name      string
		mutate    func(*databasepb.PlayerAllData)
		wantError bool
	}{
		{"仅身份可重试", func(*databasepb.PlayerAllData) {}, false},
		{"一枚金币仍是历史资产", func(p *databasepb.PlayerAllData) {
			p.PlayerDatabaseData.Currency = &componentpb.CurrencyComp{Values: []uint64{1}}
		}, true},
		{"已注册不能冒充新号", func(p *databasepb.PlayerAllData) {
			p.PlayerDatabaseData.Uint64PbComponent = &componentpb.PlayerUint64Comp{RegistrationTimestamp: 1}
		}, true},
		{"顶层未知字段保留并拒绝", func(p *databasepb.PlayerAllData) { p.ProtoReflect().SetUnknown(unknown) }, true},
		{"身份未知字段保留并拒绝", func(p *databasepb.PlayerAllData) {
			p.PlayerDatabaseData.ProfileComponent.ProtoReflect().SetUnknown(unknown)
		}, true},
		{"职业未知字段保留并拒绝", func(p *databasepb.PlayerAllData) {
			p.PlayerDatabaseData.Uint32PbComponent.ProtoReflect().SetUnknown(unknown)
		}, true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			_, rc := newSeedTestRedis(t)
			parent := newPlayerAllDataSkeleton(seedTestPlayerID)
			parent.PlayerDatabaseData.Uint32PbComponent = &componentpb.PlayerUint32Comp{Class: 3}
			parent.PlayerDatabaseData.ProfileComponent = &componentpb.PlayerProfileComp{Name: "云中君", AppearanceId: "appearance-verified", Gender: 2}
			tc.mutate(parent)
			raw, err := proto.Marshal(parent)
			if err != nil {
				t.Fatal(err)
			}
			if err := rc.Set(context.Background(), seedParentKey(seedTestPlayerID), raw, time.Hour).Err(); err != nil {
				t.Fatal(err)
			}
			_, err = beginParentLoad(context.Background(), rc, seedParentKey(seedTestPlayerID), seedTestPlayerID)
			if (err != nil) != tc.wantError {
				t.Fatalf("身份种子校验=%v，预期拒绝=%v", err, tc.wantError)
			}
			after, err := rc.Get(context.Background(), seedParentKey(seedTestPlayerID)).Bytes()
			if err != nil || string(after) != string(raw) {
				t.Fatal("只读检查修改了种子")
			}
		})
	}
}
