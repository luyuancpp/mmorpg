package kafka

// task:result 读回执到达通知的路由判定(B0-3 第 2 批,docs/handoff/launch-readiness-20261013/spec_B0-3_task_result_channel.md)。
//
// 读任务带上发起方 login 进程的回复目标 DBTask.reply_instance;publishTaskResult 据此只往该实例的频道
// task:result:notify:<id> 发一次通知,为空(旧版 login、工具)或不合法时回落全服旧频道 task:result:notify。
// 频道名与合法性规则只来自 shared/taskresult,本包不自己拼频道名。
//
// 为什么不合法时回落旧频道而不是丢弃:新版 login 也一直订着旧频道,回落照样能送达;丢弃的话等待方要等满 TTL
// (EnterGame 8s)才能靠兜底 LPOP 取回。为什么不双发:新版 login 两条频道都订着,双发它会收到两次,
// 而单发在新旧版本的任意组合下都能送达(兼容矩阵见上述规格「compat_and_rollout」)。

import (
	"time"

	"db/internal/metrics"
	"shared/taskresult"

	"github.com/zeromicro/go-zero/core/logx"
)

// taskResultNotifyTarget 返回一条读回执的通知频道,以及计数用的 target(metrics.TaskResultNotify* 之一)。
// 纯函数:只看 reply_instance 本身,不访问 Redis。
func taskResultNotifyTarget(replyInstance string) (channel, target string) {
	if replyInstance == "" {
		return taskresult.LegacyNotifyChannel, metrics.TaskResultNotifyLegacyUnset
	}
	if instanceChannel, ok := taskresult.InstanceNotifyChannel(replyInstance); ok {
		return instanceChannel, metrics.TaskResultNotifyInstance
	}
	return taskresult.LegacyNotifyChannel, metrics.TaskResultNotifyLegacyInvalid
}

// taskResultNotifyResult 把一次 PUBLISH 的返回归到计数用的 result(metrics.TaskResultNotify* 之一)。
// 订阅者数为 0 单独计 no_receiver:实例频道上它意味着发起方已不在或没订上,是频道名两端不一致的第一信号。
func taskResultNotifyResult(receivers int64, err error) string {
	switch {
	case err != nil:
		return metrics.TaskResultNotifyError
	case receivers == 0:
		return metrics.TaskResultNotifyNoReceiver
	default:
		return metrics.TaskResultNotifyOK
	}
}

// taskResultNotifyLogLimiter 给通知路径的 ERROR 日志限速(与 storeLogLimiter 分开计,同样每个 key 每 storeLogEvery 一行)。
//
// key 只能是 taskResultNotifyLogKey 的返回值,即 target × result 的常量组合,至多 9 个。logLimiter 的 map 从不回收,
// 用 task_id 或 reply_instance 这类按请求变化的值做 key 会让它无界增长 —— 那些值只进日志正文。
var taskResultNotifyLogLimiter = &logLimiter{last: map[string]time.Time{}, held: map[string]int{}}

// taskResultNotifyLogKey 拼限速 key。target 来自 taskResultNotifyTarget、result 来自 metrics.TaskResultNotify*,
// 两边都是常量,所以 key 的取值是封闭集合。
func taskResultNotifyLogKey(target, result string) string {
	return target + "/" + result
}

// logTaskResultNotify 按 key 限速打一行 ERROR,并带上上次打印之后被压掉的条数。
func logTaskResultNotify(key, format string, args ...any) {
	ok, suppressed := taskResultNotifyLogLimiter.allow(key, time.Now())
	if !ok {
		return
	}
	logx.Errorf(format+" suppressed_since_last=%d", append(args, suppressed)...)
}

// replyInstanceForLog 截断不合法的 reply_instance 再进日志:合法值至多 MaxReplyInstanceLen 字节,不合法值可能任意长。
func replyInstanceForLog(replyInstance string) string {
	if len(replyInstance) > taskresult.MaxReplyInstanceLen {
		return replyInstance[:taskresult.MaxReplyInstanceLen]
	}
	return replyInstance
}
