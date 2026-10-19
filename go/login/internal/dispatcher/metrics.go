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
//
// 各 outcome 不互斥,告警与压测汇总取分母时按下面的口径:
//   - 终态:instance_notify / legacy_notify / salvaged / timed_out。一条登记最多落在其中一个上,
//     送达率、兜底率的分母只取这四项之和。
//   - result_missing 是 notify 之后的附加结果:同一任务已计过 instance_notify 或 legacy_notify,
//     不进分母。
//   - late_notify 是附加信号,不对应任何在册登记:含真正迟到的通知(任务已计过 salvaged /
//     timed_out)、go/db 因 Kafka 重投重执行而再发一次的重复通知(任务早已送达),以及登记已被
//     Cancel 的任务的通知。不能当成纯迟到率。
//   - 不计数:Stop 时排空的登记、Stop 之后的 Register(回调收到 dispatcher stopped)、被 Cancel
//     撤销的登记(发送失败,调用方已自行收尾)。所以停机时「每条登记恰有一个终态」不严格成立。
//   - Stop 取消 ctx 后,已派出的兜底 LPOP 会因 context canceled 失败并计 timed_out;停机窗口里的
//     timed_out 不代表通知丢失。

// login_task_result_delivery_total 的 outcome 取值。
const (
	// deliveryInstanceNotify:本实例频道送达了本进程登记的任务(新版 go/db 定向发布)。
	deliveryInstanceNotify = "instance_notify"
	// deliveryLegacyNotify:旧全服频道送达了本进程登记的任务(旧版 go/db,或发布方回落旧频道)。
	// 两侧全量升级后应趋近 0;仍不为 0 说明有生产者没填 reply_instance。
	deliveryLegacyNotify = "legacy_notify"
	// deliveryLateNotify:本实例频道上来的任务已不在回调表:通知迟到(已被 TTL 兜底计过)、
	// 重复通知,或登记已被 Cancel。是附加信号而非终态,见上方口径。
	deliveryLateNotify = "late_notify"
	// deliveryResultMissing:收到通知、LPOP 结果却为空。go/db 总是先 LPUSH 再 PUBLISH,
	// 正常不会发生;最常见的成因是结果 LIST 与通知频道不在同一个 Redis 实例。
	deliveryResultMissing = "result_missing"
	// deliverySalvaged:通知没到,TTL 到期时兜底 LPOP 取回了结果。
	deliverySalvaged = "salvaged"
	// deliveryTimedOut:TTL 到期且兜底也没取到,回调收到超时错误。兜底 LPOP 报错(含停机时 ctx
	// 已取消)也计在这里;没发出去的任务由调用方 Cancel,不会留到这里。
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
		Help: "DB read-result delivery outcomes seen by this login process. Outcomes are not mutually exclusive. " +
			"Terminal (at most one per registered task; use their sum as the denominator): " +
			"instance_notify / legacy_notify (notify on the per-instance / legacy broadcast channel), " +
			"salvaged / timed_out (TTL sweep recovered / did not recover the result). " +
			"Supplementary, not in the denominator: result_missing (notified but result list empty), " +
			"late_notify (instance notify for a task no longer pending: late, duplicate or cancelled). " +
			"Cancelled registrations and those drained on Stop are not counted.",
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
