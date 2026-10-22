package kafka

import (
	"github.com/alicebob/miniredis/v2"
	"github.com/redis/go-redis/v9"
	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/conf"
	"google.golang.org/protobuf/proto"
	"shared/ownerepoch"
	"testing"

	db_config "db/internal/config"

	playerpb "proto/common/database"
	dbpb "proto/db"
)

// 启动模式缺省安全；legacy 的保留围栏行为必须由真库测试覆盖，不能用 nil store 假装验证。
func TestFenceTableMetadata(t *testing.T) {
	for _, msg := range []proto.Message{&playerpb.PlayerDatabase{}, &playerpb.PlayerDatabase_1{}} {
		if !isFenceTable(msg.ProtoReflect().Descriptor()) {
			t.Fatalf("已知玩家表缺少围栏字段: %T", msg)
		}
	}
	if isFenceTable((&dbpb.DBTask{}).ProtoReflect().Descriptor()) {
		t.Fatal("非围栏表不能放宽游标")
	}
}

func TestSetFencingModeRejectsUnknownMode(t *testing.T) {
	t.Cleanup(func() { _ = SetFencingMode(db_config.FencingModeLegacy) })
	if err := SetFencingMode(db_config.FencingModeDual); err != nil {
		t.Fatal(err)
	}
	if err := SetFencingMode("Dual"); err == nil {
		t.Fatal("模式名大小写敏感,未知值必须拒绝")
	}
	if !fencingDual.Load() {
		t.Fatal("拒绝的设置不得改动已生效的模式")
	}
}

func TestFencingYAMLPolicyRejectsAmbiguousAndUnknownModes(t *testing.T) {
	for _, test := range []struct {
		raw     string
		wantErr bool
		mode    string
	}{{"off", true, ""}, {"\"off\"", true, ""}, {"sql", true, ""}, {"legacy", false, "legacy"}, {"dual", false, "dual"},
		// 2026-10-18 起缺省与现行严格提交屏障写者同世代(存盘协议世代门禁)。
		{"", false, "dual"}} {
		t.Run(test.raw, func(t *testing.T) {
			var holder struct {
				Fencing db_config.FencingConfig `json:"Fencing,optional"`
			}
			err := conf.LoadFromYamlBytes([]byte("Fencing:\n  Mode: "+test.raw+"\n"), &holder)
			if err == nil {
				c := db_config.Config{}
				c.ServerConfig.Database.RequireTiDB = true
				c.Placement.Required = true
				c.Placement.RequireRouteReady = true
				c.ServerConfig.Fencing = holder.Fencing
				c.Normalize()
				err = c.Validate()
			}
			if test.wantErr {
				require.Error(t, err)
				return
			}
			require.NoError(t, err)
			require.Equal(t, test.mode, holder.Fencing.EffectiveMode())
			require.False(t, holder.Fencing.EpochOverride)
		})
	}
}

func TestOwnerCounterProbeUsesConfiguredCoreAndFailsClosed(t *testing.T) {
	w, _, _ := newTestWorker(t, 0)
	core := miniredis.RunT(t)
	rc := redis.NewClient(&redis.Options{Addr: core.Addr()})
	t.Cleanup(func() { _ = rc.Close() })
	w.ownerEpochRedis = rc
	require.NoError(t, w.redisClient.Set(w.ctx, ownerepoch.OwnerEpochKey(42), "1", 0).Err())
	require.NoError(t, rc.Set(w.ctx, ownerepoch.OwnerEpochKey(42), "8", 0).Err())
	regressed, err := w.ownerCounterRegressed(42, 8)
	require.NoError(t, err)
	require.False(t, regressed, "不可读错主缓存实例")
	require.NoError(t, rc.Del(w.ctx, ownerepoch.OwnerEpochKey(42)).Err())
	regressed, err = w.ownerCounterRegressed(42, 8)
	require.NoError(t, err)
	require.True(t, regressed)
	for _, raw := range []string{"01", "18446744073709551616", "garbage"} {
		require.NoError(t, rc.Set(w.ctx, ownerepoch.OwnerEpochKey(42), raw, 0).Err())
		_, err = w.ownerCounterRegressed(42, 8)
		require.Error(t, err)
	}
	core.SetError("ERR unavailable")
	_, err = w.ownerCounterRegressed(42, 8)
	require.Error(t, err)
	core.SetError("")
}
