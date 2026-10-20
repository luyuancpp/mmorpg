// Package taskresult 是 DB 读回执「到达通知」频道名与回复目标校验的唯一契约来源,
// go/db(发布方)与 login(订阅方)共用,两端不得各自拼频道名。
//
// 背景:go/db 执行完读任务后先 LPUSH task:result:{task_id}、再 PUBLISH 一条只带 task_id 的通知。
// 早期只有一个全服频道,每个 login 都收全量通知、在进程内按回调表过滤,Redis 的投递次数 =
// 读回执数 × login 实例数。现在读任务带上发起方 login 进程的回复目标(DBTask.reply_instance),
// 发布方据此只发到该实例的频道。
//
// 兼容约定(新旧版本按任意顺序滚动升级都能送达,且每条通知只发一次、不双发):
//   - 新版 login 在同一条 Pub/Sub 连接上同时订旧频道与本实例频道,旧频道订阅永久保留;
//   - 发布方对空或不合法的 reply_instance 回落旧频道(旧版 login、写任务、工具都不填)。
//
// 结果键 task:result:{task_id} 不在本包:两端目前各自拼,本包只管通知频道。
// 本包只依赖标准库。
package taskresult

// LegacyNotifyChannel 是全服共用的旧通知频道。已部署的旧版 go/db 只往这里发、旧版 login
// 只订这一条,字面值是与线上二进制之间的滚动升级契约,不许改。
const LegacyNotifyChannel = "task:result:notify"

// MaxReplyInstanceLen 是回复目标的最大字节数。36 字节的 UUID 留有余量;上限防止异常输入
// 撑大频道名。
const MaxReplyInstanceLen = 64

// ValidReplyInstance 报告 id 能否作为回复目标:长度 1..MaxReplyInstanceLen 字节,逐字节只允许
// A-Z、a-z、0-9 与 '-'。
//
// 拒绝 ':' 是为了不让实例频道与旧频道或别的 task:result:* 键名产生歧义;拒绝 '*' '?' '[' ']'
// 等 glob 元字符,是为了运维用 PSUBSCRIBE task:result:notify:* 排查时不会被频道名本身串扰。
// 手写循环而不用 regexp:发布方每条读回执都要校验一次。
func ValidReplyInstance(id string) bool {
	if len(id) == 0 || len(id) > MaxReplyInstanceLen {
		return false
	}
	for i := 0; i < len(id); i++ {
		c := id[i]
		switch {
		case c >= 'a' && c <= 'z', c >= 'A' && c <= 'Z', c >= '0' && c <= '9', c == '-':
		default:
			return false
		}
	}
	return true
}

// InstanceNotifyChannel 返回回复目标 id 对应的实例频道 task:result:notify:<id>。
// id 不合法时返回 ("", false),回落旧频道还是拒绝由调用方决定:发布方回落旧频道,
// 订阅方(进程启动时自己生成 id)视为编程错误。
func InstanceNotifyChannel(id string) (string, bool) {
	if !ValidReplyInstance(id) {
		return "", false
	}
	return LegacyNotifyChannel + ":" + id, true
}
