package metrics

// task_result_notify.go —— 读回执到达通知的投递计数(B0-3 第 2 批,
// docs/handoff/launch-readiness-20261013/spec_B0-3_task_result_channel.md)。
//
// go/db 写完 task:result:{task_id} 后按 DBTask.reply_instance 只 PUBLISH 一次:合法发发起方 login 的实例频道,
// 为空或不合法回落全服旧频道。本计数回答两个问题:定向发布占了多少(target),每次发布有没有人收(result)。
//
// 单独成文件、走自己的 sync.Once(同 pipeline.go 的理由:metrics.go 的 register() 有几条线在改)。计数入口
// CountTaskResultNotify 先自注册,所以不调 Start() 的单测(kafka 包)也能经 DefaultGatherer 读到;Start() 里再调一次,
// 让 9 条序列在第一次抓取时就以 0 出现(序列从无到 1 时 increase() 会漏掉第一次,见 registerPipeline)。
//
// label 都是封闭集合,不含 player_id / task_id / reply_instance(AGENTS.md §9):
//
//	db_task_result_notify_total{target,result}   target 3 个取值 × result 3 个取值 = 9 条,全部预置为 0
//
// 告警 deploy/k8s/task-result-notify-alerts.yaml 与 tools/scripts/stress_summarize.ps1 的 2g 段按这些字面值匹配;
// 改名或改取值要同改两处与本包的 TestTaskResultNotifySeriesPrecreated / TestTaskResultNotifyNamesMatchStressSummarize。

import (
	"sync"

	"github.com/prometheus/client_golang/prometheus"
)

// db_task_result_notify_total 的 target 取值:这条通知发到了哪里。
const (
	// TaskResultNotifyInstance:reply_instance 合法,只发发起方的实例频道 task:result:notify:<id>。
	TaskResultNotifyInstance = "instance"
	// TaskResultNotifyLegacyUnset:reply_instance 为空(旧版 login、工具),回落旧频道 task:result:notify。
	TaskResultNotifyLegacyUnset = "legacy_unset"
	// TaskResultNotifyLegacyInvalid:reply_instance 非空但不合法,回落旧频道。正常的 login 不会产生,出现即说明
	// 有生产者填错了字段。
	TaskResultNotifyLegacyInvalid = "legacy_invalid"
)

// db_task_result_notify_total 的 result 取值:PUBLISH 的结果。
const (
	// TaskResultNotifyOK:PUBLISH 成功且至少一个订阅者收到。
	TaskResultNotifyOK = "ok"
	// TaskResultNotifyNoReceiver:PUBLISH 成功但订阅者数为 0。instance 上表示发起方 login 已经不在(重启、缩容)
	// 或没订上;legacy 上表示一个订阅旧频道的 login 都没有。结果仍在列表里,由 login 的 TTL 兜底取回或随 TTL 过期。
	// 风险:PUBLISH 返回的订阅者数包含模式订阅(PSUBSCRIBE)命中的客户端。有人开着
	// PSUBSCRIBE task:result:notify:*(或更宽的模式)时,实例频道的每条通知都至少有 1 个接收者,发起方不在也计 ok,
	// no_receiver 被压成 0,NoReceiver 告警恰好在排查期间失明。排查只用 PUBSUB CHANNELS / NUMSUB 这类不建订阅的命令,
	// 观察期间 PUBSUB NUMPAT 应为 0。
	TaskResultNotifyNoReceiver = "no_receiver"
	// TaskResultNotifyError:PUBLISH 报错(Redis 不可用、超时)。同样只拉长时延,不丢结果。
	TaskResultNotifyError = "error"
)

// taskResultNotifyTargets / taskResultNotifyResults 是两个 label 的全部取值,预置序列与 stress_summarize.ps1 2g 段的
// $notifyTargets / $notifyResults 都按这个顺序。
var (
	taskResultNotifyTargets = []string{TaskResultNotifyInstance, TaskResultNotifyLegacyUnset, TaskResultNotifyLegacyInvalid}
	taskResultNotifyResults = []string{TaskResultNotifyOK, TaskResultNotifyNoReceiver, TaskResultNotifyError}
)

var (
	taskResultNotifyTotal = prometheus.NewCounterVec(prometheus.CounterOpts{
		Subsystem: subsystem,
		Name:      "task_result_notify_total",
		Help: "Arrival notifications published for DB read results, one per result (never sent twice). " +
			"target: instance (the requesting login's per-instance channel task:result:notify:<reply_instance>) | " +
			"legacy_unset (empty reply_instance, fell back to the shared channel task:result:notify) | " +
			"legacy_invalid (malformed reply_instance, fell back to the shared channel). " +
			"result: ok | no_receiver (PUBLISH reached 0 subscribers: on instance the requesting login is gone or not subscribed, " +
			"on legacy no login subscribes the shared channel) | error (PUBLISH failed). " +
			"The result list is written before the notification, so no_receiver and error only delay delivery until the login TTL salvage. " +
			"Subscriber counts include pattern (PSUBSCRIBE) subscribers: a lingering PSUBSCRIBE task:result:notify:* masks no_receiver on instance, " +
			"so inspect with PUBSUB CHANNELS / PUBSUB NUMSUB and keep PUBSUB NUMPAT at 0.",
	}, []string{"target", "result"})

	taskResultNotifyOnce sync.Once
)

// registerTaskResultNotify 注册本文件的指标,并把 3×3 条序列以 0 建出。
func registerTaskResultNotify() {
	taskResultNotifyOnce.Do(func() {
		prometheus.MustRegister(taskResultNotifyTotal)
		for _, target := range taskResultNotifyTargets {
			for _, result := range taskResultNotifyResults {
				taskResultNotifyTotal.WithLabelValues(target, result).Add(0)
			}
		}
	})
}

// CountTaskResultNotify 记一次读回执通知,target 用 TaskResultNotifyInstance / LegacyUnset / LegacyInvalid,
// result 用 TaskResultNotifyOK / NoReceiver / Error。调用方只传这些常量,label 才保持封闭。
func CountTaskResultNotify(target, result string) {
	registerTaskResultNotify()
	taskResultNotifyTotal.WithLabelValues(target, result).Inc()
}
