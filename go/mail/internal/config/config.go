// Package config 是 mail 服务的配置结构与启动期校验(docs/design/mail-system.md §7.3)。
//
// 键名与 etc/mail.yaml、tools/scripts/k8s_deploy.ps1 的 mail ConfigMap 三处逐字一致;改名必须三处同改,
// 否则 ConfigMap 里的值会被 go-zero 当未知键静默忽略 —— 这类事故没有任何报错,只会表现为"线上跑的还是默认值"。
//
// mail 是全局服务(全服一份、多副本、进程无状态,契约 §2):ZoneId 只影响 etcd 注册路径,业务代码禁止读它做分支。
package config

import (
	"errors"
	"fmt"
	"net"
	"strconv"
	"strings"
	"time"

	"mail/internal/data"

	"shared/idsegment"

	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/core/stores/redis"
	"github.com/zeromicro/go-zero/zrpc"
)

// MaxRpcTimeoutMs / MinRpcTimeoutMs 是 zrpc 服务端整体超时的区间(毫秒),与 friend 同一口径:
// 上限 = 路由服 ForwardTimeoutMs(默认 5000)− 1000(契约 §3:业务服务要先于路由服超时把 in-band 结果回出去);
// 下限保证扣掉 InBandReplyReserve 后仍有 500ms I/O 预算。
const (
	MaxRpcTimeoutMs int64 = 4000
	MinRpcTimeoutMs int64 = 1000
)

// InBandReplyReserve 是整请求业务预算相对 zrpc 服务端 Timeout 预留的余量:go-zero 的超时拦截器到点直接回
// DeadlineExceeded,handler 之后产出的 in-band kServiceUnavailable 会被丢掉(设计 §2.5,照 friend F15)。
const InBandReplyReserve = 500 * time.Millisecond

// Sweep.Mode 的两个合法取值,见 SweepConf.Mode。
const (
	SweepModeReportOnly = "report_only"
	SweepModeDelete     = "delete"
)

// AdminTokenEnv 是 MailAdmin 共享令牌所在的环境变量名(设计 §2.3)。令牌**只**从环境变量读,不进 yaml / ConfigMap
// (ConfigMap 不是 Secret):本机由 start_game.ps1 注入,K8s 由 Secret mail-admin-auth 注入(M1d)。
const AdminTokenEnv = "MMORPG_MAIL_ADMIN_TOKEN"

// AdminTokenMetadataKey 是调用方放令牌的 gRPC metadata 键(mail_admin.proto 的约定)。全小写:gRPC 在传输层把 key
// 规范成小写,直接读 map 时写错大小写就取不到。服务端(internal/server)、CLI(cmd/mailadmin)与 robot 共用这一个常量。
const AdminTokenMetadataKey = "x-mail-admin-token"

// MinAdminTokenBytes:去首尾空白后不足这么多字节的令牌视同未配置(与 assetop.MinSecretLen 同口径,设计 §2.3)。
const MinAdminTokenBytes = 32

// 各项配置的硬天花板 / 地板。它们不是调优旋钮:越过去不会报错,只会让读时合并、邮箱现数或 sweep 的开销失去依据。
const (
	// maxPersonalMailsCeiling:定向邮件现数在发件事务里按守卫行串行数一遍(§3.4),上限越大这一步越贵。
	maxPersonalMailsCeiling uint32 = 1000
	// maxActiveSystemMailsCeiling:ListMails 每次都要把有效系统邮件集读出来合并(§3.1),它就是那次读的 LIMIT。
	maxActiveSystemMailsCeiling uint32 = 500
	// maxTtlDaysCeiling:邮件最长有效期的上限。有效期越长,有效系统邮件集越容易顶到上限、唯一键行留得越久。
	maxTtlDaysCeiling uint32 = 365
	// maxRetentionDays:防 now − RetentionDays 回绕(设计 §4.6)。
	maxRetentionDays = 36500
	// maxAllowedCurrencyTypes:附件货币条数的协议上限是 4(§3.5),白名单比它长没有意义。
	maxAllowedCurrencyTypes = 16
)

// Config 是 mail 服务的全部配置。
//
// 共享 Redis **不在这里重新声明**:嵌入的 zrpc.RpcServerConf 已带 `Redis redis.RedisKeyConf`(yaml 顶层 `Redis:`),
// 再声明同名字段会在加载期报 "conflict key Redis"。mail 没有私有 key,只读跨运行时契约 key player:session:{id}
// (推送前找收件人所在 gate,§4.5),所以只有这一个句柄。
type Config struct {
	zrpc.RpcServerConf

	// ZoneId:只影响 etcd 注册路径 <Prefix>/zone/<z>/...。必填且拒 0;业务代码禁止读它做分支。
	ZoneId uint32 `json:",optional"`

	// LeaseTTL:etcd 节点注册租约 TTL(秒),即崩溃场景的切换窗口(路由服 PickRandom 不看连接状态)。
	LeaseTTL int64 `json:",default=60"`

	// MetricsListenAddr:Prometheus /metrics 监听地址,留空关闭。mail 用 :9240(设计 §7.1)。
	// 必须是 yaml 顶层单行:go_services.ps1 -Zone 的端口位移正则只认这个形状。
	MetricsListenAddr string `json:",optional"`

	// KillSwitchPrefix:RPC 级热关停规则在 etcd 里的前缀,留空用 /mmorpg/killswitch/。
	KillSwitchPrefix string `json:",optional"`

	// MySQL:独占库 mmorpg_mail(D-14)。结构化字段,DSN 由 svc.BuildDSN 统一拼(带严格模式与会话级 RC)。
	MySQL MySQLConf `json:"MySQL"`

	// Schema:建表策略,见 SchemaConf.AutoMigrate。整段可缺失。
	Schema SchemaConf `json:",optional"`

	// Kafka:S2C 推送(kafkautil.PushToPlayer → gate-cmd_g<N>)的 broker 地址。Brokers 为空合法:推送降级为不推
	// (at-most-once,红点真相是 ListMails 的 unread_count),横幅会显示出来。
	Kafka KafkaConf `json:"Kafka"`

	// DataServiceRpc:领 mail_id / sys_mail_id 号段(AllocateIdSegment)的 data_service 客户端。
	// 写 Key: dataservice.rpc 不违反 D-13:D-13 禁的是指向**全局服务**的 RpcClient Key,data_service 按 zone 部署
	// (同 go/trade/etc/trade.yaml)。不标 optional。
	DataServiceRpc zrpc.RpcClientConf `json:"DataServiceRpc"`

	// IdSegment:mail_id / sys_mail_id 号段(biz_tag=mail,两种 id 共用,§3.7)。标 optional 只为让"整段没写"
	// 落到 Validate 给出可读错误;Enabled 必须显式 true,mail 没有 snowflake 回退。
	IdSegment idsegment.Conf `json:"IdSegment,optional"`

	// Mail:邮件业务的上限。刻意不标 optional(整段漏写时 go-zero 下钻回填 default,见 friend 的 FriendConf 注释),
	// 但 AllowedCurrencyTypes 没有可信的默认值,Validate 要求显式写。
	Mail MailConf `json:"Mail"`

	// Sweep:过期邮件的后台物理清理参数,见 SweepConf。
	Sweep SweepConf `json:",optional"`
}

// MySQLConf 是 mmorpg_mail 的连接参数。
type MySQLConf struct {
	Host string `json:"Host"`
	User string `json:"User"`
	// Password 可以为空(本地免密账号);值只进 DSN,绝不打日志、不进启动横幅。
	Password    string `json:"Password,optional"`
	DBName      string `json:"DBName"`
	MaxOpenConn int    `json:"MaxOpenConn,default=20"`
	MaxIdleConn int    `json:"MaxIdleConn,default=5"`
}

// KafkaConf 是推送 topic 的 broker 地址(形状与 friend / guild 一致)。
type KafkaConf struct {
	Brokers []string `json:"Brokers,optional"`
}

// SchemaConf 建表策略(D-14 第 4 条)。AutoMigrate 为 nil(没写)或 true:启动时跑 schemamigrate.Up;
// false:只跑只读 Plan,有待执行语句或需人工项即拒启。用 *bool 的理由见 friend 的同名结构。
type SchemaConf struct {
	AutoMigrate *bool `json:",optional"`
}

// MailConf 是邮件业务的上限(设计 §3.1、§3.4、§3.5)。改动必须同步 k8s ConfigMap。
type MailConf struct {
	// MaxPersonalMailsPerPlayer:单玩家**有效**定向邮件(未删、未过期)的上限,超限拒收回 MailRecipientBoxFull
	// (已拍板 100,§11 第 5 条)。系统邮件不计入。
	MaxPersonalMailsPerPlayer uint32 `json:",default=100"`

	// MaxActiveSystemMails:有效系统邮件数的软上限(发件时检查)与 ListMails 读系统有效集的 LIMIT(硬上限)。
	// 读时合并要求它有界(§3.1)。
	MaxActiveSystemMails uint32 `json:",default=100"`

	// MaxPersonalMailTtlDays / MaxSystemMailTtlDays:发件时 expire_ms 最多比服务端当前时刻晚这么多天。
	// 系统邮件强制有界的有效期,有效系统邮件集才不会越积越多(§3.1)。
	MaxPersonalMailTtlDays uint32 `json:",default=30"`
	MaxSystemMailTtlDays   uint32 `json:",default=30"`

	// AllowedCurrencyTypes:附件货币类型白名单(§3.5)。权威来源是 C++ cpp/libs/modules/currency/constants/currency.h
	// 的 kCurrencyMax(当前 0 金币 / 1 钻石 / 2 绑钻);scene 的 IsCurrencyAmountValid 要求 currency_type < kCurrencyMax。
	// 刻意不给 default:没有可信的默认值,漏写就拒启,而不是悄悄放行所有类型。
	AllowedCurrencyTypes []uint32 `json:",optional"`
}

// SweepConf 是过期邮件的物理清理参数(设计 §4.6)。整段标 optional 只为让"没写 Sweep 段"落到 Validate 给出
// 可读错误:go-zero 不下钻整段 optional 且未出现的嵌套结构,default 不回填,所以 yaml 与 ConfigMap 必须写全四个键。
type SweepConf struct {
	// Mode:report_only 只出指标不删;delete 真删。默认 report_only:清理的是权威数据,新环境先观察候选行数。
	Mode string `json:",default=report_only,options=report_only|delete"`

	// Interval:两轮 sweep 之间的间隔(首轮带随机抖动,M1c)。
	Interval time.Duration `json:",default=5m"`

	// RetentionDays:过期后再保留多少天才物理删(供客服查询)。**下界 1**:它同时是
	//   - 发件幂等键寿命的余量(唯一键行最短活 expire_ms + RetentionDays,§2.4),
	//   - "系统邮件 state 行的条件 upsert(只认 expire_ms > now)"与"sweep(只删 expire_ms < now − RetentionDays)"
	//     两个集合不相交的间隔(§4.2)—— 取 0 时两者可能交错出孤儿 state 行。
	RetentionDays int `json:",default=7"`

	// BatchLimit:单条 DELETE 的行数上限(DELETE … LIMIT)。
	BatchLimit int `json:",default=1000"`
}

// RequestBudget 返回一次请求内全部 I/O(MySQL、号段、Redis、Kafka)共用的业务预算。Validate 保证结果恒为正。
func (c Config) RequestBudget() time.Duration {
	return time.Duration(c.Timeout)*time.Millisecond - InBandReplyReserve
}

// ShouldAutoMigrate 是启动路径跑 Up 还是只跑 Plan 的唯一判据:没写 = Up。
func (c Config) ShouldAutoMigrate() bool {
	return c.Schema.AutoMigrate == nil || *c.Schema.AutoMigrate
}

// ListenHostPort 把 ListenOn 拆成 host 与端口号(net.SplitHostPort 正确处理 "[::]:50900")。
func (c *Config) ListenHostPort() (string, uint32, error) {
	host, portStr, err := net.SplitHostPort(c.ListenOn)
	if err != nil {
		return "", 0, fmt.Errorf("ListenOn 必须是 host:port,得到 %q: %w", c.ListenOn, err)
	}
	port, err := strconv.ParseUint(portStr, 10, 16)
	if err != nil || port == 0 {
		return "", 0, fmt.Errorf("ListenOn 端口非法: %q", c.ListenOn)
	}
	return host, uint32(port), nil
}

// IsRelaxedMode 报告 go-zero Mode 是否为 dev / test(与 friend / trade / login 同一判据)。
// mail 用它决定两件事(M-9):是否允许发带附件的邮件、是否接受 sender_kind=ROBOT 的发件方。go-zero 的 Mode 默认是 pro。
func IsRelaxedMode(mode string) bool {
	return mode == service.DevMode || mode == service.TestMode
}

// ResolveAdminToken 从环境变量读 MailAdmin 令牌(设计 §2.3)。返回去首尾空白后的值;
// 去空白后不足 MinAdminTokenBytes 字节 → 返回 "",即 MailAdmin 整体停用(不是"免鉴权")。
// getenv 由调用方传入(os.Getenv),单测可注入。令牌值绝不进日志、指标、错误文本。
func ResolveAdminToken(getenv func(string) string) string {
	token := strings.TrimSpace(getenv(AdminTokenEnv))
	if len(token) < MinAdminTokenBytes {
		return ""
	}
	return token
}

// HasRpcTarget 报告一个 zrpc 客户端配置有没有可拨的目标(同 go/trade 的判据)。
func HasRpcTarget(c zrpc.RpcClientConf) bool {
	return c.Etcd.Key != "" || len(c.Endpoints) > 0 || c.Target != ""
}

// Validate 做启动期整体校验(fail-fast)。conf.MustLoad 会自动调用它(go-zero v1.10.0)。
// 每一条拒绝的都是"进程能起来、但行为静默出错"的配置,理由写在各分支旁边。
func (c *Config) Validate() error {
	// ZoneId=0:注册路径变成 <Prefix>/zone/0/...,路由服看不见本节点,零报错。
	if c.ZoneId == 0 {
		return errors.New("ZoneId 必须非 0(K8s 全局服务用 ConfigMap 注入的 CurrentZoneId)")
	}
	if c.ListenOn == "" {
		return errors.New("ListenOn 不能为空")
	}
	if _, _, err := c.ListenHostPort(); err != nil {
		return err
	}
	if c.Timeout < MinRpcTimeoutMs || c.Timeout > MaxRpcTimeoutMs {
		return fmt.Errorf("Timeout(%d ms)必须在 [%d, %d] 内:上限是路由服 ForwardTimeoutMs − 1000(契约 §3),"+
			"下限保证扣掉 %v 回包余量后仍有 I/O 预算", c.Timeout, MinRpcTimeoutMs, MaxRpcTimeoutMs, InBandReplyReserve)
	}
	if len(c.Etcd.Hosts) == 0 {
		return errors.New("Etcd.Hosts 不能为空(节点注册与 killswitch 都依赖 etcd)")
	}
	// Etcd.Key 非空:go-zero 会额外注册 mail.rpc,并被 go_services.ps1 -Zone 按 .z<N> 切分 —— 与 noderegistry
	// 的全服一份语义冲突(D-13)。yaml 必须显式写 `Key: ""`,整行省略会在加载期 Fatal。
	if c.Etcd.Key != "" {
		return fmt.Errorf("Etcd.Key 必须留空(得到 %q):全局服务不注册 go-zero key,契约 D-13", c.Etcd.Key)
	}
	if c.LeaseTTL <= 0 {
		return fmt.Errorf("LeaseTTL 必须为正数,得到 %d", c.LeaseTTL)
	}

	if c.MySQL.Host == "" || c.MySQL.User == "" || c.MySQL.DBName == "" {
		return errors.New("MySQL.Host / MySQL.User / MySQL.DBName 都不能为空")
	}
	// DBName 不是 mmorpg_mail:schemamigrate 会把邮件的表建进别的服务的库,而且建表本身会成功(D-14 第 3 条)。
	if c.MySQL.DBName != data.DatabaseName {
		return fmt.Errorf("MySQL.DBName 必须是 %q(得到 %q):mail 独占该库,表不许建进别的库(D-14)",
			data.DatabaseName, c.MySQL.DBName)
	}
	if c.MySQL.MaxOpenConn <= 0 || c.MySQL.MaxIdleConn <= 0 {
		return fmt.Errorf("MySQL.MaxOpenConn(%d)与 MySQL.MaxIdleConn(%d)必须为正数",
			c.MySQL.MaxOpenConn, c.MySQL.MaxIdleConn)
	}

	// 共享 Redis 是推送前读 player:session:{id} 的唯一句柄(§4.5)。Host 为空 = 推送永远读不到会话。
	// 必须是 node:这个契约 key 没有 hash tag,共享库禁 Cluster(契约 §4)。
	if c.Redis.Host == "" {
		return errors.New("Redis.Host(共享库)不能为空:它是读契约 key player:session:{id} 的唯一句柄(推送找收件人所在 gate)")
	}
	if c.Redis.Type == redis.ClusterType {
		return errors.New("Redis.Type 不能是 cluster:共享会话库禁 Cluster(契约 §4,player:session:{id} 没有 hash tag)")
	}

	if !HasRpcTarget(c.DataServiceRpc) {
		return errors.New("DataServiceRpc 必须配置 Etcd.Key / Endpoints / Target 之一:mail_id / sys_mail_id 都从 data_service 号段领")
	}
	if !c.IdSegment.Enabled {
		return errors.New("IdSegment.Enabled 必须为 true:mail_id / sys_mail_id 只由号段发(biz_tag=mail),mail 没有 snowflake 回退")
	}
	if c.IdSegment.FallbackToSnowflake {
		return errors.New("IdSegment.FallbackToSnowflake 必须为 false:mail 没有 snowflake 节点可回退")
	}

	if err := c.Mail.validate(); err != nil {
		return err
	}
	return c.Sweep.validate()
}

func (m MailConf) validate() error {
	bounded := []struct {
		name    string
		value   uint32
		ceiling uint32
	}{
		{"Mail.MaxPersonalMailsPerPlayer", m.MaxPersonalMailsPerPlayer, maxPersonalMailsCeiling},
		{"Mail.MaxActiveSystemMails", m.MaxActiveSystemMails, maxActiveSystemMailsCeiling},
		{"Mail.MaxPersonalMailTtlDays", m.MaxPersonalMailTtlDays, maxTtlDaysCeiling},
		{"Mail.MaxSystemMailTtlDays", m.MaxSystemMailTtlDays, maxTtlDaysCeiling},
	}
	for _, b := range bounded {
		// 0 都是"功能静默变形":上限 0 = 谁都发不进去;有效期 0 = 任何 expire_ms 都越界。
		if b.value == 0 || b.value > b.ceiling {
			return fmt.Errorf("%s 必须在 [1, %d] 内,得到 %d", b.name, b.ceiling, b.value)
		}
	}
	if len(m.AllowedCurrencyTypes) == 0 {
		return errors.New("Mail.AllowedCurrencyTypes 必须显式写(当前应为 [0, 1, 2],权威来源 cpp/libs/modules/currency/constants/currency.h)")
	}
	if len(m.AllowedCurrencyTypes) > maxAllowedCurrencyTypes {
		return fmt.Errorf("Mail.AllowedCurrencyTypes 至多 %d 项,得到 %d 项", maxAllowedCurrencyTypes, len(m.AllowedCurrencyTypes))
	}
	seen := make(map[uint32]struct{}, len(m.AllowedCurrencyTypes))
	for _, t := range m.AllowedCurrencyTypes {
		if _, dup := seen[t]; dup {
			return fmt.Errorf("Mail.AllowedCurrencyTypes 有重复项 %d", t)
		}
		seen[t] = struct{}{}
	}
	return nil
}

// AllowsCurrency 报告某个货币类型是否在附件白名单内。
func (m MailConf) AllowsCurrency(currencyType uint32) bool {
	for _, t := range m.AllowedCurrencyTypes {
		if t == currencyType {
			return true
		}
	}
	return false
}

func (s SweepConf) validate() error {
	// Mode 空串 = 整个 Sweep 段没写、default 没回填,Interval / RetentionDays / BatchLimit 也全是 0。
	if s.Mode != SweepModeReportOnly && s.Mode != SweepModeDelete {
		return fmt.Errorf("Sweep.Mode 必须是 %q 或 %q,得到 %q(整段 Sweep 未写时 go-zero 不回填 default)",
			SweepModeReportOnly, SweepModeDelete, s.Mode)
	}
	// Interval ≤ 0:time.NewTicker 会 panic。
	if s.Interval <= 0 {
		return fmt.Errorf("Sweep.Interval 必须为正数,得到 %v", s.Interval)
	}
	// RetentionDays 下界 1(理由见 SweepConf.RetentionDays);上界防 now − RetentionDays 回绕。
	if s.RetentionDays < 1 || s.RetentionDays > maxRetentionDays {
		return fmt.Errorf("Sweep.RetentionDays 必须在 [1, %d] 内,得到 %d(它是幂等键寿命的余量,也是 upsert 与 sweep 不相交的间隔)",
			maxRetentionDays, s.RetentionDays)
	}
	// BatchLimit ≤ 0:DELETE 不带 LIMIT,一条长语句锁住整张表。
	if s.BatchLimit <= 0 {
		return fmt.Errorf("Sweep.BatchLimit 必须为正数,得到 %d", s.BatchLimit)
	}
	return nil
}
