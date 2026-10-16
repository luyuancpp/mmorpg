package metrics

import (
	"testing"

	"github.com/prometheus/client_golang/prometheus"
)

// TestSetKafkaConsumerUpCreatesChildSeries 锁死告警的前提:
// kafka_consumer_up{consumer=...} 这条子序列在**消费者还没起来**时就必须存在、且值为 0。
//
// Prometheus 的 *Vec 只有 WithLabelValues 之后才有 child series。data_service 最该告警的
// 故障态恰恰是"EnsureTopics 一直失败 → 两条消费者一条都没起来":如果那时序列根本不存在,
// `data_service_kafka_consumer_up == 0` 的告警永远不会触发,监控上与"服务没部署"无从区分。
// 所以 startKafkaConsumers 在第一次 Start 尝试之前先按 false 打一次点。
func TestSetKafkaConsumerUpCreatesChildSeries(t *testing.T) {
	const consumer = "metrics_test_consumer"
	SetKafkaConsumerUp(consumer, false)

	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather: %v", err)
	}
	for _, mf := range families {
		if mf.GetName() != "data_service_kafka_consumer_up" {
			continue
		}
		for _, m := range mf.GetMetric() {
			for _, l := range m.GetLabel() {
				if l.GetName() == "consumer" && l.GetValue() == consumer {
					if got := m.GetGauge().GetValue(); got != 0 {
						t.Fatalf("kafka_consumer_up{consumer=%q} = %v, want 0", consumer, got)
					}
					return
				}
			}
		}
		t.Fatalf("kafka_consumer_up has no child series for consumer=%q", consumer)
	}
	t.Fatal("metric family data_service_kafka_consumer_up is not registered")
}

// TestPrimeRollbackGuildCheckCreatesAllSeries 锁死 M1(docs/design/guild-phase2/07-rollback-fail-closed.md §7.9.1):
// PrimeRollbackGuildCheck 之后,rollback_guild_check_total 的 scope × result 封闭集合 27 条子序列**全部存在且为 0**。
//
// 放行回档(divergent_accepted)与写后分歧(post_write_*)是进程一辈子可能只发生一次的事件,
// 告警是 `increase(...) > 0`:序列在第一次 Inc 之前不存在的话,那一次恰好只有一个样本,告警是哑的。
func TestPrimeRollbackGuildCheckCreatesAllSeries(t *testing.T) {
	PrimeRollbackGuildCheck()

	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather: %v", err)
	}
	for _, mf := range families {
		if mf.GetName() != "data_service_rollback_guild_check_total" {
			continue
		}
		seen := make(map[string]float64, len(mf.GetMetric()))
		for _, m := range mf.GetMetric() {
			var scope, result string
			for _, l := range m.GetLabel() {
				switch l.GetName() {
				case "scope":
					scope = l.GetValue()
				case "result":
					result = l.GetValue()
				}
			}
			seen[scope+"/"+result] = m.GetCounter().GetValue()
		}
		if len(seen) != len(rollbackGuildScopes)*len(rollbackGuildResults) || len(seen) != 27 {
			t.Fatalf("rollback_guild_check_total has %d series after priming, want 27: %v", len(seen), seen)
		}
		for _, scope := range rollbackGuildScopes {
			for _, result := range rollbackGuildResults {
				v, ok := seen[scope+"/"+result]
				if !ok {
					t.Fatalf("series scope=%q result=%q missing after priming", scope, result)
				}
				if v != 0 {
					t.Fatalf("series scope=%q result=%q = %v after priming, want 0", scope, result, v)
				}
			}
		}
		if _, ok := seen[RollbackGuildScopePlayer+"/"+RollbackGuildResultDivergentAccepted]; !ok {
			t.Fatal(`series scope="player",result="divergent_accepted" missing`)
		}
		return
	}
	t.Fatal("metric family data_service_rollback_guild_check_total is not registered")
}

// TestPrimeRouteCreatesAllSeries 锁死映射持久化第 1a 批的两组告警前提:PrimeRoute 之后,
// route_register_total{outcome} 的 12 条与 route_publish_total{result} 的 8 条子序列**全部存在且为 0**。
// fenced / refused_unknown_row / legacy_conflict 这类结局可能一辈子只发生几次,序列不预先存在,
// `increase(...) > 0` 恰好漏掉第一次(同 TestPrimeRollbackGuildCheckCreatesAllSeries)。
func TestPrimeRouteCreatesAllSeries(t *testing.T) {
	PrimeRoute()

	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather: %v", err)
	}
	want := map[string]struct {
		label  string
		values []string
		count  int
	}{
		"data_service_route_register_total": {"outcome", routeRegisterOutcomes, 12},
		"data_service_route_publish_total":  {"result", routePublishResults, 8},
	}
	found := map[string]bool{}
	for _, mf := range families {
		spec, ok := want[mf.GetName()]
		if !ok {
			continue
		}
		found[mf.GetName()] = true
		seen := make(map[string]float64, len(mf.GetMetric()))
		for _, m := range mf.GetMetric() {
			for _, l := range m.GetLabel() {
				if l.GetName() == spec.label {
					seen[l.GetValue()] = m.GetCounter().GetValue()
				}
			}
		}
		if len(spec.values) != spec.count || len(seen) != spec.count {
			t.Fatalf("%s has %d series after priming (closed set lists %d), want %d: %v",
				mf.GetName(), len(seen), len(spec.values), spec.count, seen)
		}
		for _, v := range spec.values {
			got, ok := seen[v]
			if !ok {
				t.Fatalf("%s{%s=%q} missing after priming", mf.GetName(), spec.label, v)
			}
			if got != 0 {
				t.Fatalf("%s{%s=%q} = %v after priming, want 0", mf.GetName(), spec.label, v, got)
			}
		}
	}
	for name := range want {
		if !found[name] {
			t.Fatalf("metric family %s is not registered", name)
		}
	}
}

// TestPrimeKafkaConsumerCreatesAllSeries 锁死审计管道告警(deploy/k8s/data-service-audit-alerts.yaml)的前提:
// PrimeKafkaConsumer 之后,这条消费者的 messages_total 6 个 outcome、lag_probe_total 2 个 result、
// isolation_total 2 个 result **全部存在且为 0**。坏消息跳过 / 批量兜底停机这类结局可能一辈子只发生几次,
// 序列不预先存在,`increase(...) > 0` 恰好漏掉第一次(同 TestPrimeRouteCreatesAllSeries)。
// 用一个只属于本用例的 consumer 名,免得同包其它用例打过的点让"恰好为 0"失真。
func TestPrimeKafkaConsumerCreatesAllSeries(t *testing.T) {
	const consumer = "metrics_test_prime_consumer"
	PrimeKafkaConsumer(consumer)

	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather: %v", err)
	}
	want := map[string]struct {
		label  string
		values []string
		count  int
	}{
		"data_service_kafka_consumer_messages_total":  {"outcome", kafkaConsumerOutcomes, 6},
		"data_service_kafka_consumer_lag_probe_total": {"result", kafkaLagProbeResults, 2},
		"data_service_kafka_consumer_isolation_total": {"result", kafkaIsolationResults, 2},
	}
	found := map[string]bool{}
	for _, mf := range families {
		spec, ok := want[mf.GetName()]
		if !ok {
			continue
		}
		found[mf.GetName()] = true
		seen := map[string]float64{}
		for _, m := range mf.GetMetric() {
			var mine bool
			var value string
			for _, l := range m.GetLabel() {
				switch l.GetName() {
				case "consumer":
					mine = l.GetValue() == consumer
				case spec.label:
					value = l.GetValue()
				}
			}
			if mine {
				seen[value] = m.GetCounter().GetValue()
			}
		}
		if len(spec.values) != spec.count || len(seen) != spec.count {
			t.Fatalf("%s{consumer=%q} has %d series after priming (closed set lists %d), want %d: %v",
				mf.GetName(), consumer, len(seen), len(spec.values), spec.count, seen)
		}
		for _, v := range spec.values {
			got, ok := seen[v]
			if !ok {
				t.Fatalf("%s{consumer=%q,%s=%q} missing after priming", mf.GetName(), consumer, spec.label, v)
			}
			if got != 0 {
				t.Fatalf("%s{consumer=%q,%s=%q} = %v after priming, want 0", mf.GetName(), consumer, spec.label, v, got)
			}
		}
	}
	for name := range want {
		if !found[name] {
			t.Fatalf("metric family %s is not registered", name)
		}
	}
}

// TestSetKafkaConsumerLagWritesAllThreeGauges 锁死 lag 探针一次成功探测写出的三条 gauge:合计积压、单分区最大积压、
// 保留期缺口,各落在自己的序列上(三个 int64 参数挨着,串位了编译器不会报)。DataServiceAuditLagHigh 比的是单分区最大值:
// broker 的 retention.bytes 按分区执行,拿合计去比会在积压集中于一个分区时漏报。
// 三条都不由 PrimeKafkaConsumer 预置:探针没成功过,就不该有一个看着像「积压为 0」的数。
func TestSetKafkaConsumerLagWritesAllThreeGauges(t *testing.T) {
	const consumer = "metrics_test_lag_consumer"
	SetKafkaConsumerLag(consumer, 1751, 1750, 7)

	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatalf("gather: %v", err)
	}
	want := map[string]float64{
		"data_service_kafka_consumer_lag_messages":               1751,
		"data_service_kafka_consumer_lag_max_partition_messages": 1750,
		"data_service_kafka_consumer_retention_gap_messages":     7,
	}
	found := map[string]bool{}
	for _, mf := range families {
		wantValue, ok := want[mf.GetName()]
		if !ok {
			continue
		}
		for _, m := range mf.GetMetric() {
			for _, l := range m.GetLabel() {
				if l.GetName() != "consumer" || l.GetValue() != consumer {
					continue
				}
				found[mf.GetName()] = true
				if got := m.GetGauge().GetValue(); got != wantValue {
					t.Fatalf("%s{consumer=%q} = %v, want %v", mf.GetName(), consumer, got, wantValue)
				}
			}
		}
	}
	for name := range want {
		if !found[name] {
			t.Fatalf("%s has no series for consumer=%q", name, consumer)
		}
	}
}

// TestSQLPoolCollectorDeclaresRouteOnlyUnderDBFirst 钉住 data_service 连接池类别的声明集合(G2b):
// redis 模式(includeRoute=false)只声明四个 store 的池,db_first 才多出 player_route —— 理由同 PrimeRoute,
// redis 模式下不装配 route store,不要一组恒为 0、看着像在跑的序列。常量 label sql_client="data_service"。
func TestSQLPoolCollectorDeclaresRouteOnlyUnderDBFirst(t *testing.T) {
	for _, tc := range []struct {
		includeRoute bool
		want         []string
	}{
		{includeRoute: false, want: []string{PoolSnapshot, PoolTransactionLog, PoolIdSegment, PoolPlayerName}},
		{includeRoute: true, want: []string{PoolSnapshot, PoolTransactionLog, PoolIdSegment, PoolPlayerName, PoolPlayerRoute}},
	} {
		reg := prometheus.NewPedanticRegistry()
		if err := reg.Register(NewSQLPoolCollector(nil, tc.includeRoute)); err != nil {
			t.Fatalf("includeRoute=%v: register: %v", tc.includeRoute, err)
		}
		families, err := reg.Gather()
		if err != nil {
			t.Fatalf("includeRoute=%v: gather: %v", tc.includeRoute, err)
		}
		seen := map[string]bool{}
		for _, mf := range families {
			if mf.GetName() != "mmorpg_sql_pool_pools" {
				continue
			}
			for _, m := range mf.GetMetric() {
				var pool, client string
				for _, l := range m.GetLabel() {
					switch l.GetName() {
					case "pool":
						pool = l.GetValue()
					case "sql_client":
						client = l.GetValue()
					}
				}
				if client != "data_service" {
					t.Fatalf("includeRoute=%v: pool %q has sql_client=%q, want data_service", tc.includeRoute, pool, client)
				}
				seen[pool] = true
			}
		}
		if len(seen) != len(tc.want) {
			t.Fatalf("includeRoute=%v: declared pools = %v, want %v", tc.includeRoute, seen, tc.want)
		}
		for _, pool := range tc.want {
			if !seen[pool] {
				t.Fatalf("includeRoute=%v: pool %q not declared (got %v)", tc.includeRoute, pool, seen)
			}
		}
	}
}
