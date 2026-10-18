package dispatcher

import (
	"sync"

	"github.com/prometheus/client_golang/prometheus"
)

// 读回执送达结果计数 login_task_result_delivery_total{outcome}。
//
// 直接用 client_golang 注册,不用 go-zero metric:后者在 go-zero 的 Prometheus 开关打开之前
// (以及单测里)Inc 是空操作,测试读不到值。注册放进 sync.Once 自举,构造 dispatcher 时就把
// 全部 outcome 以 0 建出序列:序列从无到 1 时 increase() 会漏掉第一次,告警与压测汇总需要
// 启动即有序列。
//
// outcome 是下列常量的封闭集合,不带 player_id / task_id(AGENTS.md §9)。

// login_task_result_delivery_total 的 outcome 取值。
const (
	// deliveryInstanceNotify:本实例频道送达了本进程登记的任务(新版 go/db 定向发布)。
	deliveryInstanceNotify = "instance_notify"
	// deliveryLegacyNotify:旧全服频道送达了本进程登记的任务(旧版 go/db,或发布方回落旧频道)。
	// 两侧全量升级后应趋近 0;仍不为 0 说明有生产者没填 reply_instance。
	deliveryLegacyNotify = "legacy_notify"
	// deliveryLateNotify:本实例频道上来的任务已不在回调表(已超时,或已被 TTL 兜底取走)。
	deliveryLateNotify = "late_notify"
	// deliveryResultMissing:收到通知、LPOP 结果却为空。go/db 总是先 LPUSH 再 PUBLISH,
	// 正常不会发生;最常见的成因是结果 LIST 与通知频道不在同一个 Redis 实例。
	deliveryResultMissing = "result_missing"
	// deliverySalvaged:通知没到,TTL 到期时兜底 LPOP 取回了结果。
	deliverySalvaged = "salvaged"
	// deliveryTimedOut:TTL 到期且兜底也没取到,回调收到超时错误。
	deliveryTimedOut = "timed_out"
)

var deliveryOutcomes = []string{
	deliveryInstanceNotify, deliveryLegacyNotify, deliveryLateNotify,
	deliveryResultMissing, deliverySalvaged, deliveryTimedOut,
}

var (
	deliveryTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Namespace: "login_task_result",
		Name:      "delivery_total",
		Help: "DB read-result delivery outcomes seen by this login process. " +
			"instance_notify / legacy_notify: notify on the per-instance / legacy broadcast channel; " +
			"late_notify: instance notify for a task no longer pending; result_missing: notified but result list empty; " +
			"salvaged / timed_out: TTL sweep recovered / did not recover the result.",
	}, []string{"outcome"})

	deliveryOnce sync.Once
)

// registerDeliveryMetrics 注册计数器并预置全部 outcome 序列,可重复调用。
func registerDeliveryMetrics() {
	deliveryOnce.Do(func() {
		prometheus.MustRegister(deliveryTotal)
		for _, outcome := range deliveryOutcomes {
			deliveryTotal.WithLabelValues(outcome).Add(0)
		}
	})
}

// countDelivery 记一次送达结果,outcome 只能取 delivery* 常量。
func countDelivery(outcome string) {
	registerDeliveryMetrics()
	deliveryTotal.WithLabelValues(outcome).Inc()
}
