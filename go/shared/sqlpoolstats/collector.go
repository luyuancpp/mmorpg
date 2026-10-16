// Package sqlpoolstats 把 database/sql 连接池的 DBStats 按「池类别」聚合后导出给 Prometheus。
//
// 为什么按类别聚合而不是一池一条序列:go/db 的额外落点库按需打开、库数不设上限(player-storage-placement.md §6.1),
// 按库名打 label,序列数会随库数增长;总账(tools/scripts/k8s_deploy.ps1 Get-MysqlConnectionLedger)关心的
// 本来就是「这一类一共能开多少、开了多少」。
// 为什么不用 client_golang 自带的 collectors.NewDBStatsCollector:它一池一个 Collector、label 是 db_name,
// 同一 db_name 注册两次直接报错,也做不了按类别求和。
//
// 导出的指标(namespace mmorpg、subsystem sql_pool;变量 label pool;常量 label sql_client):
//
//	mmorpg_sql_pool_pools                          gauge   类别内的池个数
//	mmorpg_sql_pool_max_open_connections           gauge   MaxOpenConnections 之和;任一池不限(0)时为 +Inf
//	mmorpg_sql_pool_open_connections               gauge   OpenConnections 之和
//	mmorpg_sql_pool_in_use_connections             gauge   InUse 之和
//	mmorpg_sql_pool_idle_connections               gauge   Idle 之和
//	mmorpg_sql_pool_wait_count_total               counter WaitCount 之和
//	mmorpg_sql_pool_wait_duration_seconds_total    counter WaitDuration 之和(秒)
//	mmorpg_sql_pool_max_idle_closed_total          counter MaxIdleClosed 之和
//	mmorpg_sql_pool_max_idle_time_closed_total     counter MaxIdleTimeClosed 之和
//	mmorpg_sql_pool_max_lifetime_closed_total      counter MaxLifetimeClosed 之和
//
// 常量 label 叫 sql_client 而不是 service:kube-prometheus 的 ServiceMonitor 会加 service 目标 label,
// 同名时抓到的 label 被改名为 exported_service,告警表达式会静默落空。
//
// counter 是类别内求和:只要池不被移除就单调。**移除池会让求和回退,被 Prometheus 当成 counter reset**;
// 以后若给调用方的池加回收,要同时改聚合口径(例如把已关闭池的终值累进一个常量)。
package sqlpoolstats

import (
	"database/sql"
	"fmt"
	"math"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/zeromicro/go-zero/core/logx"
)

// 不 import "time":WaitDuration 是 time.Duration,int64(...) 是类型转换,不需要导入 time 包。

// OtherPool 收容类别不在声明集合里的样本(调用方笔误),不让它们扩张 label 基数。
// 它只在确有这类样本时输出,不预置 0:本来就不该出现,预置会让面板上多一组看着像在用的序列。
const OtherPool = "other"

const (
	metricNamespace = "mmorpg"
	metricSubsystem = "sql_pool"
	poolLabel       = "pool"
	clientLabel     = "sql_client"
)

// Sample 是一次抓取时一个连接池的快照。
type Sample struct {
	Pool  string // 池类别:低基数封闭集合,绝不放库名 / player_id(AGENTS §9)
	Stats sql.DBStats
}

// Source 在每次抓取时被调用一次,返回此刻全部连接池的快照。
// 契约:并发安全;不做网络 I/O、不阻塞(sql.DB.Stats 只取一把内存锁);可以返回空。
type Source func() []Sample

// collector 构造后只读:Collect 可能被并发调用(两路抓取同时到),累加器一律是 Collect 内的局部变量。
type collector struct {
	service  string
	pools    []string
	declared map[string]struct{}
	source   Source

	poolsDesc             *prometheus.Desc
	maxOpenDesc           *prometheus.Desc
	openDesc              *prometheus.Desc
	inUseDesc             *prometheus.Desc
	idleDesc              *prometheus.Desc
	waitCountDesc         *prometheus.Desc
	waitDurationDesc      *prometheus.Desc
	maxIdleClosedDesc     *prometheus.Desc
	maxIdleTimeClosedDesc *prometheus.Desc
	maxLifetimeClosedDesc *prometheus.Desc
}

// NewCollector 构造收集器。service 进常量 label sql_client;pools 是声明的池类别,每次抓取都为每个类别输出一组序列
// (没有池时全 0、pools 为 0)。参数非法(service 或某个 pool 为空、pool 重复、pool 等于 OtherPool)直接 panic:
// 这是启动期的编程错误,与 prometheus.MustRegister 同语义。source 为 nil 视为没有池。
func NewCollector(service string, pools []string, source Source) prometheus.Collector {
	if service == "" {
		panic("sqlpoolstats: service (sql_client label) must not be empty")
	}
	declared := make(map[string]struct{}, len(pools))
	for _, pool := range pools {
		if pool == "" {
			panic(fmt.Sprintf("sqlpoolstats: %s declares an empty pool name", service))
		}
		if pool == OtherPool {
			panic(fmt.Sprintf("sqlpoolstats: %s declares the reserved pool name %q", service, OtherPool))
		}
		if _, dup := declared[pool]; dup {
			panic(fmt.Sprintf("sqlpoolstats: %s declares pool %q twice", service, pool))
		}
		declared[pool] = struct{}{}
	}

	constLabels := prometheus.Labels{clientLabel: service}
	desc := func(name, help string) *prometheus.Desc {
		return prometheus.NewDesc(prometheus.BuildFQName(metricNamespace, metricSubsystem, name), help,
			[]string{poolLabel}, constLabels)
	}
	return &collector{
		service:  service,
		pools:    append([]string(nil), pools...),
		declared: declared,
		source:   source,

		poolsDesc:             desc("pools", "Number of database/sql connection pools in this pool class."),
		maxOpenDesc:           desc("max_open_connections", "Sum of MaxOpenConnections over the pools in this class; +Inf when any pool is unlimited."),
		openDesc:              desc("open_connections", "Sum of established connections (in use + idle) over the pools in this class."),
		inUseDesc:             desc("in_use_connections", "Sum of connections currently in use over the pools in this class."),
		idleDesc:              desc("idle_connections", "Sum of idle connections over the pools in this class."),
		waitCountDesc:         desc("wait_count_total", "Total number of connections waited for, summed over the pools in this class."),
		waitDurationDesc:      desc("wait_duration_seconds_total", "Total time blocked waiting for a new connection, summed over the pools in this class."),
		maxIdleClosedDesc:     desc("max_idle_closed_total", "Total connections closed due to SetMaxIdleConns, summed over the pools in this class."),
		maxIdleTimeClosedDesc: desc("max_idle_time_closed_total", "Total connections closed due to SetConnMaxIdleTime, summed over the pools in this class."),
		maxLifetimeClosedDesc: desc("max_lifetime_closed_total", "Total connections closed due to SetConnMaxLifetime, summed over the pools in this class."),
	}
}

// Describe 实现 prometheus.Collector。
func (c *collector) Describe(ch chan<- *prometheus.Desc) {
	ch <- c.poolsDesc
	ch <- c.maxOpenDesc
	ch <- c.openDesc
	ch <- c.inUseDesc
	ch <- c.idleDesc
	ch <- c.waitCountDesc
	ch <- c.waitDurationDesc
	ch <- c.maxIdleClosedDesc
	ch <- c.maxIdleTimeClosedDesc
	ch <- c.maxLifetimeClosedDesc
}

// poolTotals 是一个类别内的累加器。
type poolTotals struct {
	pools             int
	maxOpen           int64
	unlimited         bool
	open              int64
	inUse             int64
	idle              int64
	waitCount         int64
	waitNanos         int64
	maxIdleClosed     int64
	maxIdleTimeClosed int64
	maxLifetimeClosed int64
}

func (t *poolTotals) add(s sql.DBStats) {
	t.pools++
	if s.MaxOpenConnections <= 0 {
		// database/sql 里 0 = 不限:这一类的上限就是没有上限,求和没有意义。
		t.unlimited = true
	} else {
		t.maxOpen += int64(s.MaxOpenConnections)
	}
	t.open += int64(s.OpenConnections)
	t.inUse += int64(s.InUse)
	t.idle += int64(s.Idle)
	t.waitCount += s.WaitCount
	// 按整数纳秒累加,输出时才换算成秒:浮点加法不满足结合律,而调用方给样本的顺序不固定
	// (go/db 的 PoolSamples 遍历 map)。按秒累加时同一组值两次抓取可能差 1 ULP,
	// 变小的那次会被 Prometheus 当成 counter reset。
	t.waitNanos += int64(s.WaitDuration)
	t.maxIdleClosed += s.MaxIdleClosed
	t.maxIdleTimeClosed += s.MaxIdleTimeClosed
	t.maxLifetimeClosed += s.MaxLifetimeClosed
}

// Collect 实现 prometheus.Collector。
//
// 先把 source 的快照完整取出来再输出任何一条序列:source panic 时本次抓取本收集器**一条都不输出**,
// 不能输出 0 —— counter 先归 0 再回到真值会被 increase() 当成一次暴涨,误触发等待告警。
// panic 在这里 recover:go/db 是存盘主路径,观测代码不能把进程带崩。
func (c *collector) Collect(ch chan<- prometheus.Metric) {
	samples, ok := c.snapshot()
	if !ok {
		return
	}

	totals := make(map[string]*poolTotals, len(c.pools)+1)
	for _, pool := range c.pools {
		totals[pool] = &poolTotals{}
	}
	for _, s := range samples {
		pool := s.Pool
		if _, known := c.declared[pool]; !known {
			pool = OtherPool
		}
		t := totals[pool]
		if t == nil {
			t = &poolTotals{}
			totals[pool] = t
		}
		t.add(s.Stats)
	}

	for _, pool := range c.pools {
		c.emit(ch, pool, totals[pool])
	}
	if other := totals[OtherPool]; other != nil {
		c.emit(ch, OtherPool, other)
	}
}

// snapshot 调一次 source;panic 时记 ERROR 并返回 ok=false。
func (c *collector) snapshot() (samples []Sample, ok bool) {
	if c.source == nil {
		return nil, true
	}
	defer func() {
		if r := recover(); r != nil {
			logx.Errorf("[sqlpoolstats] sql_client=%s pool source panicked; skipping this scrape (no series rather than zeros, so counters do not look reset): %v",
				c.service, r)
			samples, ok = nil, false
		}
	}()
	return c.source(), true
}

func (c *collector) emit(ch chan<- prometheus.Metric, pool string, t *poolTotals) {
	maxOpen := float64(t.maxOpen)
	if t.unlimited {
		maxOpen = math.Inf(1)
	}
	waitSeconds := float64(t.waitNanos) / 1e9
	gauge := func(desc *prometheus.Desc, v float64) {
		ch <- prometheus.MustNewConstMetric(desc, prometheus.GaugeValue, v, pool)
	}
	counter := func(desc *prometheus.Desc, v float64) {
		ch <- prometheus.MustNewConstMetric(desc, prometheus.CounterValue, v, pool)
	}
	gauge(c.poolsDesc, float64(t.pools))
	gauge(c.maxOpenDesc, maxOpen)
	gauge(c.openDesc, float64(t.open))
	gauge(c.inUseDesc, float64(t.inUse))
	gauge(c.idleDesc, float64(t.idle))
	counter(c.waitCountDesc, float64(t.waitCount))
	counter(c.waitDurationDesc, waitSeconds)
	counter(c.maxIdleClosedDesc, float64(t.maxIdleClosed))
	counter(c.maxIdleTimeClosedDesc, float64(t.maxIdleTimeClosed))
	counter(c.maxLifetimeClosedDesc, float64(t.maxLifetimeClosed))
}
