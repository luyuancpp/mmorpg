package dbguard

import (
	"bytes"
	"encoding/base64"
	"errors"
	"testing"

	"github.com/luyuancpp/proto2mysql/pbconv"

	assetpb "proto/common/asset"
	component "proto/common/component"
	dbpb "proto/common/database"

	"google.golang.org/protobuf/encoding/protowire"
	"google.golang.org/protobuf/proto"
)

// 用真实生成消息与合法未知字段构造大小边界，保留完整实例身份，不靠手算代替序列化。
func padGeneratedMessage(t *testing.T, message proto.Message, target int) []byte {
	t.Helper()
	message.ProtoReflect().SetUnknown(nil)
	padding := target - proto.Size(message) - 8
	for attempt := 0; attempt < 4; attempt++ {
		if padding < 0 {
			t.Fatalf("目标大小不足以容纳消息: target=%d", target)
		}
		unknown := protowire.AppendTag(nil, 60001, protowire.BytesType)
		unknown = protowire.AppendBytes(unknown, make([]byte, padding))
		message.ProtoReflect().SetUnknown(unknown)
		raw, err := proto.Marshal(message)
		if err != nil {
			t.Fatal(err)
		}
		if len(raw) == target {
			return raw
		}
		padding += target - len(raw)
	}
	t.Fatalf("无法构造精确消息大小: target=%d actual=%d", target, proto.Size(message))
	return nil
}

func fullAssetIdentityWindow(t *testing.T, snapshots bool) *dbpb.PlayerDatabase {
	t.Helper()
	const maximum = ^uint64(0)
	const maximumSeq = maximum - 2048
	const watermark = maximumSeq - 1024
	ledger := &component.PlayerAssetOpLedgerComp{}
	for stream := assetpb.AssetOpStream_ASSET_OP_STREAM_GUILD_DEBIT; stream <= assetpb.AssetOpStream_ASSET_OP_STREAM_SYSTEM_CREDIT; stream++ {
		entry := &component.AssetOpStreamLedger{
			Stream: stream, StreamEpoch: maximum, Watermark: watermark, MaxSeq: maximumSeq,
			SeenBits: make([]uint64, 16), AppliedBits: make([]uint64, 16),
		}
		for word := range entry.SeenBits {
			entry.SeenBits[word] = maximum
			if word < 15 {
				entry.AppliedBits[word] = maximum
			}
		}
		for index := uint64(0); index < 1024; index++ {
			seq := watermark + index + 1
			entry.Receipts = append(entry.Receipts, &component.AssetOpReceipt{
				Seq: seq, CorrelationId: maximum - index, RequestDigest: bytes.Repeat([]byte{byte(stream)}, 32),
			})
			if index < 960 {
				entry.PartialSeqs = append(entry.PartialSeqs, seq)
			} else {
				entry.Rejections = append(entry.Rejections, &component.AssetOpRejection{Seq: seq, ReasonTipId: ^uint32(0)})
			}
		}
		if snapshots && stream == assetpb.AssetOpStream_ASSET_OP_STREAM_TRADE_DEBIT {
			// 四份各 64 KiB 的快照正好打满场景约定的 256 KiB 原件总量。
			for receipt := 0; receipt < 4; receipt++ {
				snapshot := &assetpb.AssetSnapshot{}
				for item := 0; item < 4; item++ {
					id := maximum - uint64(receipt*4+item)
					instance := &component.ItemComp{ItemId: id, ConfigId: 1, Size: 1}
					snapshot.Items = append(snapshot.Items, &assetpb.AssetItemInstance{
						ItemUuid: id, ConfigId: 1, StackSize: 1, InstanceProto: padGeneratedMessage(t, instance, 16360),
					})
				}
				last := snapshot.Items[len(snapshot.Items)-1]
				// 内层长度变化可能跨过外层 message 的长度 varint 边界，
				// 必须按完整快照反馈校准，不能只补一次原始字节差额。
				for attempt := 0; attempt < 4 && proto.Size(snapshot) != 64<<10; attempt++ {
					target := len(last.InstanceProto) + (64<<10 - proto.Size(snapshot))
					if target > 16<<10 {
						t.Fatalf("实例超过场景 16 KiB 上限: %d", target)
					}
					last.InstanceProto = padGeneratedMessage(t, &component.ItemComp{ItemId: last.ItemUuid, ConfigId: 1, Size: 1}, target)
				}
				if proto.Size(snapshot) != 64<<10 {
					t.Fatalf("快照边界未打满: %d", proto.Size(snapshot))
				}
				entry.Receipts[receipt].Snapshot = snapshot
			}
		}
		ledger.Streams = append(ledger.Streams, entry)
	}
	if snapshots {
		total := 0
		for _, stream := range ledger.Streams {
			for _, receipt := range stream.Receipts {
				if receipt.Snapshot != nil {
					total += proto.Size(receipt.Snapshot)
				}
			}
		}
		if total != 256<<10 {
			t.Fatalf("四份托管原件必须打满 256 KiB 总量: %d", total)
		}
	}
	return &dbpb.PlayerDatabase{PlayerId: maximum, AssetOpLedger: ledger}
}

func assetIdentityBudgetGuard() *Guard {
	return NewGuard(Limits{MaxColumnBytes: 256 << 10, MaxRowBytes: 1 << 20, PerTable: map[string]TableLimit{
		"player_database": {ColumnBytes: map[string]int64{"asset_op_ledger": 1 << 20}},
	}}, nil)
}

func TestAssetLedgerFullWindowAndSnapshotFitsOnlySpecificColumn(t *testing.T) {
	for _, snapshots := range []bool{false, true} {
		name := "五流纯身份"
		if snapshots {
			name = "五流身份及256KiB原件"
		}
		t.Run(name, func(t *testing.T) {
			message := fullAssetIdentityWindow(t, snapshots)
			raw, err := proto.Marshal(message.AssetOpLedger)
			if err != nil {
				t.Fatal(err)
			}
			stored := len(base64.StdEncoding.EncodeToString(raw))
			field := message.ProtoReflect().Descriptor().Fields().ByName("asset_op_ledger")
			if field == nil {
				t.Fatal("生成玩家消息缺少资产账本字段")
			}
			sqlValue, err := pbconv.SerializeFieldValue(message, field)
			if err != nil {
				t.Fatalf("正式 SQL 参数序列化失败: %v", err)
			}
			sqlBytes, ok := sqlValue.(string)
			if !ok || len(sqlBytes) != len(raw) || !bytes.Equal([]byte(sqlBytes), raw) || len(sqlBytes) > stored {
				t.Fatalf("SQL 参数必须为裸 protobuf 且不超过兼容计费: type=%T raw=%d SQL=%d conservative=%d",
					sqlValue, len(raw), len(sqlBytes), stored)
			}
			t.Logf("账本原始protobuf=%dB，真实SQL参数=%dB，兼容base64保守计费=%dB", len(raw), len(sqlBytes), stored)
			if stored <= 256<<10 || stored > 1<<20 {
				t.Fatalf("边界未覆盖旧闸拒绝和新闸放行: stored=%d", stored)
			}
			old := NewGuard(Limits{}, nil)
			if _, err := old.Check("player_database", message); err == nil {
				t.Fatal("原 256 KiB 列闸必须真实拒绝此合法有界账本")
			}
			result, err := assetIdentityBudgetGuard().Check("player_database", message)
			if err != nil {
				t.Fatalf("特定账本列预算应放行: %v", err)
			}
			if len(result.Columns) != 1 || result.Columns[0].Column != "asset_op_ledger" ||
				result.Columns[0].PayloadBytes != int64(len(raw)) || result.Columns[0].StoredBytes != int64(stored) ||
				result.Columns[0].Limit != 1<<20 || result.RowLimit != 1<<20 {
				t.Fatalf("量测或精准预算错误: %+v", result)
			}
			message.StressTestProbe = &component.PlayerStressTestProbe{TestSig: make([]byte, 256<<10)}
			result, err = assetIdentityBudgetGuard().Check("player_database", message)
			if err == nil {
				t.Fatal("同表其他组件不得跟随账本一起放宽")
			}
			for _, column := range result.Columns {
				if column.Column == "stress_test_probe" && (column.Limit != 256<<10 || column.Level != LevelReject) {
					t.Fatalf("其他组件的原列闸被改变: %+v", column)
				}
			}
		})
	}
}

func TestAssetLedgerColumnOverrideStillEnforcesOriginalPlayerRowBudget(t *testing.T) {
	message := fullAssetIdentityWindow(t, true)
	message.StressTestProbe = &component.PlayerStressTestProbe{}
	padGeneratedMessage(t, message.StressTestProbe, 192<<10)
	result, err := assetIdentityBudgetGuard().Check("player_database", message)
	if err == nil || result.RowLevel != LevelReject || result.RowLimit != 1<<20 || result.TotalStoredBytes <= result.RowLimit {
		t.Fatalf("列覆盖不得绕过整行总预算: %+v err=%v", result, err)
	}
	for _, column := range result.Columns {
		if column.Level == LevelReject || column.StoredBytes > column.Limit {
			t.Fatalf("本例必须只有整行超限，单列都须合规: %+v", column)
		}
	}
}

// recordingObserver 收集上报,断言 histogram / counter 真的被喂了数据。
type recordingObserver struct {
	blob map[string]int64
	row  map[string]int64
	gate []string
}

func newRecordingObserver() *recordingObserver {
	return &recordingObserver{blob: map[string]int64{}, row: map[string]int64{}}
}

func (o *recordingObserver) ObserveBlobBytes(table, column string, bytes int64) {
	o.blob[table+"."+column] = bytes
}

func (o *recordingObserver) ObserveRowBytes(table string, bytes int64) {
	o.row[table] = bytes
}

func (o *recordingObserver) CountGate(table, column, level string) {
	o.gate = append(o.gate, table+"."+column+"="+level)
}

// snapshotWithData 造一条 player_snapshot,data 是唯一的 bytes blob 列。
func snapshotWithData(n int) proto.Message {
	return &dbpb.PlayerSnapshot{PlayerId: 1, Data: make([]byte, n)}
}

func TestMeasureRetainsConservativeBase64Budget(t *testing.T) {
	// 当前 SQL 参数是裸字节；生产闸与既有指标仍沿用更保守的 base64 长度计费。
	// 此回归防止列预算修补顺带改变计费口径，真实 SQL 编码由满窗口用例另行对照。
	const payload = 3000
	result := Measure("player_snapshot", snapshotWithData(payload))

	var data *ColumnMeasure
	for i := range result.Columns {
		if result.Columns[i].Column == "data" {
			data = &result.Columns[i]
		}
	}
	if data == nil {
		t.Fatalf("data column not measured, got %+v", result.Columns)
	}
	if data.PayloadBytes != payload {
		t.Fatalf("payload bytes = %d, want %d", data.PayloadBytes, payload)
	}
	want := int64(base64.StdEncoding.EncodedLen(payload))
	if data.StoredBytes != want {
		t.Fatalf("stored bytes = %d, want base64 length %d", data.StoredBytes, want)
	}
	if result.TotalStoredBytes != want {
		t.Fatalf("row total = %d, want %d", result.TotalStoredBytes, want)
	}
}

func TestMeasureSkipsScalarColumns(t *testing.T) {
	// 标量列(uint64 / string)不落 MEDIUMBLOB,不该被算进 blob 预算。
	result := Measure("player_snapshot", &dbpb.PlayerSnapshot{
		Id: 7, PlayerId: 8, ZoneId: 1, Reason: "some very long reason string", Operator: "gm",
	})
	if len(result.Columns) != 0 {
		t.Fatalf("scalar-only row must have no blob columns, got %+v", result.Columns)
	}
	if result.TotalStoredBytes != 0 {
		t.Fatalf("scalar-only row total = %d, want 0", result.TotalStoredBytes)
	}
}

func TestGuardThreeTiers(t *testing.T) {
	const limit = 4096
	// warn 水位 = 4096 × 0.8 = 3276.8 → 3276
	cases := []struct {
		name    string
		payload int
		want    Level
	}{
		// base64 长度 = ceil(n/3)*4
		{"正常", 1200, LevelOK},     // 1600B < 3276B
		{"预警", 2500, LevelWarn},   // 3336B ∈ [3276, 4096]
		{"超限", 3200, LevelReject}, // 4268B > 4096B
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			obs := newRecordingObserver()
			g := NewGuard(Limits{MaxColumnBytes: limit, MaxRowBytes: 1 << 20, WarnRatio: 0.8}, obs)
			result, err := g.Check("player_snapshot", snapshotWithData(tc.payload))
			if result.Level != tc.want {
				t.Fatalf("level = %v, want %v (stored=%d limit=%d)", result.Level, tc.want, result.TotalStoredBytes, limit)
			}
			if tc.want == LevelReject {
				var tooLarge *ErrBlobTooLarge
				if !errors.As(err, &tooLarge) {
					t.Fatalf("over-limit write must be rejected, got err=%v", err)
				}
			} else if err != nil {
				t.Fatalf("level %v must not reject: %v", tc.want, err)
			}
			if _, ok := obs.blob["player_snapshot.data"]; !ok {
				t.Fatal("histogram must be fed on every write, not only on violations")
			}
			if _, ok := obs.row["player_snapshot"]; !ok {
				t.Fatal("row histogram must be fed on every write")
			}
			if tc.want == LevelOK && len(obs.gate) != 0 {
				t.Fatalf("normal write must not increment the gate counter: %v", obs.gate)
			}
			if tc.want != LevelOK && len(obs.gate) == 0 {
				t.Fatal("warn/reject must increment the gate counter")
			}
		})
	}
}

func TestGuardRowTotalTripsIndependently(t *testing.T) {
	// 单列都不超,但一行里多个 blob 列加起来超了 —— 这正是 player_database
	// 有 9 个 blob 列时的真实风险,单列闸拦不住。
	msg := &dbpb.PlayerDatabase{
		PlayerId:        1,
		SkillList:       &component.PlayerSkillListComp{},
		StressTestProbe: &component.PlayerStressTestProbe{TestSig: make([]byte, 900)},
		Currency:        &component.CurrencyComp{},
	}
	obs := newRecordingObserver()
	g := NewGuard(Limits{MaxColumnBytes: 1 << 20, MaxRowBytes: 1000, WarnRatio: 0.8}, obs)

	result, err := g.Check("player_database", msg)
	if result.RowLevel != LevelReject {
		t.Fatalf("row level = %v, want reject (total=%d limit=%d)", result.RowLevel, result.TotalStoredBytes, result.RowLimit)
	}
	if err == nil {
		t.Fatal("row-total violation must reject the write")
	}
	for _, c := range result.Columns {
		if c.Level != LevelOK {
			t.Fatalf("no single column should trip here, but %s did (%d/%d)", c.Column, c.StoredBytes, c.Limit)
		}
	}
	var sawRowGate bool
	for _, g := range obs.gate {
		if g == "player_database."+RowColumnLabel+"=reject" {
			sawRowGate = true
		}
	}
	if !sawRowGate {
		t.Fatalf("row-level violation must be counted under %s, got %v", RowColumnLabel, obs.gate)
	}
}

func TestGuardReportOnlyDoesNotReject(t *testing.T) {
	g := NewGuard(Limits{MaxColumnBytes: 16, MaxRowBytes: 16, WarnRatio: 0.8, ReportOnly: true}, nil)
	result, err := g.Check("player_snapshot", snapshotWithData(4096))
	if err != nil {
		t.Fatalf("report-only mode must not reject: %v", err)
	}
	if result.Level != LevelReject {
		t.Fatalf("report-only must still classify as reject, got %v", result.Level)
	}
}

func TestGuardPerTableOverride(t *testing.T) {
	// player_snapshot 天然就大,必须能单独放宽而不影响 player_database。
	g := NewGuard(Limits{
		MaxColumnBytes: 1024,
		MaxRowBytes:    1024,
		WarnRatio:      0.8,
		PerTable:       map[string]TableLimit{"player_snapshot": {MaxColumnBytes: 1 << 20, MaxRowBytes: 1 << 20}},
	}, nil)

	if _, err := g.Check("player_snapshot", snapshotWithData(4096)); err != nil {
		t.Fatalf("per-table override must apply: %v", err)
	}
	if _, err := g.Check("player_database", &dbpb.PlayerDatabase{
		PlayerId:        1,
		StressTestProbe: &component.PlayerStressTestProbe{TestSig: make([]byte, 4096)},
	}); err == nil {
		t.Fatal("tables without an override must keep the global limit")
	}
}

func TestNewGuardRejectsZeroLimits(t *testing.T) {
	// 零值 Limits 直接构造出来的闸如果按字面执行,上限就是 0 = 全部拒写。
	g := NewGuard(Limits{}, nil)
	if g.Limits().MaxColumnBytes <= 0 || g.Limits().MaxRowBytes <= 0 || g.Limits().WarnRatio <= 0 {
		t.Fatalf("zero limits must be backfilled, got %+v", g.Limits())
	}
	if _, err := g.Check("player_database", &dbpb.PlayerDatabase{PlayerId: 1}); err != nil {
		t.Fatalf("an empty row must never be rejected: %v", err)
	}
}
