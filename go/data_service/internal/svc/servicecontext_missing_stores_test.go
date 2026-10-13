package svc

import (
	"context"
	"net"
	"reflect"
	"testing"

	"data_service/internal/config"
	"data_service/internal/routing"
	"data_service/internal/store"

	"github.com/zeromicro/go-zero/core/stores/redis"
)

// MissingStores 是 Store.Required 拒启判定的唯一依据:四个 store 各自独立上报,顺序固定(日志与告警可比对)。
// 这里只需要"是不是 nil",所以直接用零值结构,不连库。
func TestMissingStoresReportsEachNilStore(t *testing.T) {
	all := []string{"snapshot", "transaction_log", "id_segment", "player_name"}
	if got := (&ServiceContext{}).MissingStores(); !reflect.DeepEqual(got, all) {
		t.Fatalf("a context with no stores must report all four in order, got %v", got)
	}
}

// 下面几个桩只为让 MissingStores 看到"非 nil":嵌入接口本身(零值)即满足接口,方法从不会被调用 ——
// MissingStores 只判空,调到任何方法都是测试写错了,nil 接口会当场 panic 暴露出来。
type stubSnapshotStore struct{ SnapshotStore }
type stubTxLogStore struct{ TransactionLogStore }
type stubIdSegmentStore struct{ IdSegmentStore }
type stubPlayerNameStore struct{ PlayerNameStore }

// stubPlayerRouteStore 是 PlayerRouteStore 的最小实现(RegisterHome + Close),不嵌入接口:
// 两个方法都写实,以免哪天 MissingStores 之外的路径误调它时 nil 接口 panic 掩盖真正的问题。
type stubPlayerRouteStore struct{}

func (stubPlayerRouteStore) RegisterHome(context.Context, store.RouteRegisterRequest) (store.RouteRegisterResult, error) {
	return store.RouteRegisterResult{}, nil
}
func (stubPlayerRouteStore) Close() error { return nil }

// db_first 下 player_route 是必需的:K8s 各档 Store.Required=true,它缺席时 main 必须拒启 ——
// 这条是"db_first 装配失败即拒启"的唯一判据(规格 d 节 MissingStores)。redis 模式根本不装配它,
// 不能算缺,否则今天所有档位都会拒启。顺序断言与上一个用例同口径:player_route 恒在末尾。
func TestMissingStoresReportsPlayerRouteOnlyUnderDBFirst(t *testing.T) {
	fourReady := func(c config.Config, route PlayerRouteStore) *ServiceContext {
		return &ServiceContext{
			Config:           c,
			SnapshotStore:    stubSnapshotStore{},
			TxLogStore:       stubTxLogStore{},
			IdSegmentStore:   stubIdSegmentStore{},
			PlayerNameStore:  stubPlayerNameStore{},
			PlayerRouteStore: route,
		}
	}
	withMode := func(mode string) config.Config {
		var c config.Config
		c.RouteStore.Mode = mode
		return c
	}

	cases := []struct {
		name string
		ctx  *ServiceContext
		want []string
	}{
		{"db_first without route store", fourReady(withMode(config.RouteStoreModeDBFirst), nil), []string{"player_route"}},
		{"db_first with route store", fourReady(withMode(config.RouteStoreModeDBFirst), stubPlayerRouteStore{}), nil},
		{"redis without route store", fourReady(withMode(config.RouteStoreModeRedis), nil), nil},
		// 空 Mode = 没写 RouteStore 段(Normalize 之前的形态):等同 redis,同样不能算缺。
		{"empty mode without route store", fourReady(withMode(""), nil), nil},
		// 五项都缺时 player_route 排在四个老 store 之后,告警比对的顺序不变。
		{"db_first with nothing assembled", &ServiceContext{Config: withMode(config.RouteStoreModeDBFirst)},
			[]string{"snapshot", "transaction_log", "id_segment", "player_name", "player_route"}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := tc.ctx.MissingStores(); !reflect.DeepEqual(got, tc.want) {
				t.Fatalf("MissingStores() = %v, want %v", got, tc.want)
			}
		})
	}
}

// db_first 装配前的 WAITAOF 能力探针(routing.ProbeWaitAOF):mapping Redis 确认不了落盘就不装配 route store,
// 让配置错误挡在启动期(Store.Required=true 时经 MissingStores 拒启),而不是留到每一次 W1 都失败、
// 再经 go-zero 熔断器拖累同一 Redis 上的登录读。这里只用一个必然连不上的地址,不需要真 Redis:
//   - 默认 WaitAOFNumLocal=1 → 探针失败 → newPlayerRouteStore 返回 nil(在开库之前就返回,不碰 MySQL);
//   - 两项都显式写 0 → 不发 WAITAOF,探针直接放行(连地址都不碰)。
func TestNewPlayerRouteStoreRefusedWhenWaitAOFProbeFails(t *testing.T) {
	var c config.Config
	c.RouteStore = config.RouteStoreConfig{Mode: config.RouteStoreModeDBFirst}.Normalize()
	c.MappingRedis = redis.RedisConf{Host: closedLoopbackAddr(t), Type: "node"}

	if err := routing.ProbeWaitAOF(context.Background(), c.MappingRedis, c.RouteStore); err == nil {
		t.Fatal("WaitAOFNumLocal=1 against an unreachable mapping redis must fail the probe")
	}
	if got := newPlayerRouteStore(c, store.MySQLConfig{}, true); got != nil {
		t.Fatalf("route store must not be assembled when the WAITAOF probe fails, got %T", got)
	}

	zero := 0
	c.RouteStore.WaitAOFNumLocal, c.RouteStore.WaitAOFNumReplicas = &zero, &zero
	if err := routing.ProbeWaitAOF(context.Background(), c.MappingRedis, c.RouteStore); err != nil {
		t.Fatalf("WaitAOF 0/0 sends no WAITAOF and must not touch the mapping redis, got %v", err)
	}
}

// closedLoopbackAddr 返回一个刚关掉的本机端口:连它必然被拒绝。
func closedLoopbackAddr(t *testing.T) string {
	t.Helper()
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen: %v", err)
	}
	addr := l.Addr().String()
	if err := l.Close(); err != nil {
		t.Fatalf("close listener: %v", err)
	}
	return addr
}
