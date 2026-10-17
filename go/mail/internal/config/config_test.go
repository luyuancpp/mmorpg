package config

import (
	"os"
	"strings"
	"testing"
	"time"

	"mail/internal/data"

	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/service"
	"github.com/zeromicro/go-zero/zrpc"
)

// etcYaml 用相对路径:`go test ./...` 的工作目录是被测包目录,这条路径在任何 checkout 位置都成立。
const etcYaml = "../../etc/mail.yaml"

// TestEtcYamlContractValues 钉住 etc/mail.yaml 的关键值。段名 / 键名写错时 go-zero 会把字段静默保持零值,
// 配置加载不报任何错;这里是第二道网,顺带证明 yaml 能通过 conf.Load + Validate。
func TestEtcYamlContractValues(t *testing.T) {
	var c Config
	if err := conf.Load(etcYaml, &c); err != nil {
		t.Fatalf("load %s: %v", etcYaml, err)
	}
	if err := c.Validate(); err != nil {
		t.Fatalf("%s 必须能通过 Validate: %v", etcYaml, err)
	}

	if c.Name != "mail.rpc" {
		t.Errorf("Name = %q, want mail.rpc", c.Name)
	}
	if _, port, err := c.ListenHostPort(); err != nil || port != 50900 {
		t.Errorf("ListenOn = %q (port=%d err=%v), want port 50900(设计 §7.1)", c.ListenOn, port, err)
	}
	if c.Timeout != 4000 {
		t.Errorf("Timeout = %d, want 4000(契约 §3 上限)", c.Timeout)
	}
	if c.Mode != service.DevMode {
		t.Errorf("Mode = %q, want dev(本地发附件 / ROBOT 发件方都依赖 dev)", c.Mode)
	}
	if c.ZoneId != 1 || c.LeaseTTL != 60 {
		t.Errorf("ZoneId / LeaseTTL = %d / %d, want 1 / 60", c.ZoneId, c.LeaseTTL)
	}
	if c.Etcd.Key != "" || len(c.Etcd.Hosts) == 0 {
		t.Errorf("Etcd = %+v, want 非空 Hosts 且 Key 为空串(D-13)", c.Etcd)
	}
	if c.MetricsListenAddr != ":9240" {
		t.Errorf("MetricsListenAddr = %q, want :9240(设计 §7.1)", c.MetricsListenAddr)
	}
	if c.KillSwitchPrefix != "" {
		t.Errorf("KillSwitchPrefix = %q, want 空串", c.KillSwitchPrefix)
	}
	if c.Redis.Host == "" || c.Redis.Type != "node" {
		t.Errorf("Redis = %s/%s, want 非空 Host 且 Type=node(共享会话库禁 Cluster)", c.Redis.Host, c.Redis.Type)
	}
	if c.MySQL.DBName != data.DatabaseName {
		t.Errorf("MySQL.DBName = %q, want %q(D-14)", c.MySQL.DBName, data.DatabaseName)
	}
	if !c.ShouldAutoMigrate() {
		t.Error("本地 dev 应启动期自动建表(Schema.AutoMigrate: true)")
	}
	if c.DataServiceRpc.Etcd.Key != "dataservice.rpc" || !c.DataServiceRpc.NonBlock {
		t.Errorf("DataServiceRpc = Key %q NonBlock %v, want dataservice.rpc / true(照 go/trade)",
			c.DataServiceRpc.Etcd.Key, c.DataServiceRpc.NonBlock)
	}
	if !c.IdSegment.Enabled || c.IdSegment.FallbackToSnowflake {
		t.Errorf("IdSegment = %+v, want Enabled=true FallbackToSnowflake=false", c.IdSegment)
	}

	// 上限逐项钉死:它们是硬上限,被人"顺手调大"就等于取消了这层保护。
	// MailConf 含切片不能整体比较,逐项比。
	if m := c.Mail; m.MaxPersonalMailsPerPlayer != 100 || m.MaxActiveSystemMails != 100 ||
		m.MaxPersonalMailTtlDays != 30 || m.MaxSystemMailTtlDays != 30 {
		t.Errorf("Mail = %+v, want 上限 100 / 100、有效期 30 / 30 天", c.Mail)
	}
	if len(c.Mail.AllowedCurrencyTypes) != 3 || !c.Mail.AllowsCurrency(0) || !c.Mail.AllowsCurrency(1) || !c.Mail.AllowsCurrency(2) {
		t.Errorf("Mail.AllowedCurrencyTypes = %v, want [0 1 2](currency.h 的 kCurrencyMax = 3)", c.Mail.AllowedCurrencyTypes)
	}
	wantSweep := SweepConf{Mode: SweepModeReportOnly, Interval: 5 * time.Minute, RetentionDays: 7, BatchLimit: 1000}
	if c.Sweep != wantSweep {
		t.Errorf("Sweep = %+v, want %+v(新环境先 report_only)", c.Sweep, wantSweep)
	}

	// MailAdmin 的请求体(收件人 + 正文)不整包进 stat 日志;ConfigMap 也必须带同一份(契约 §7)。
	ignored := map[string]bool{}
	for _, m := range c.Middlewares.StatConf.IgnoreContentMethods {
		ignored[m] = true
	}
	for _, m := range []string{"/mailpb.MailAdmin/SendPersonalMail", "/mailpb.MailAdmin/SendSystemMail"} {
		if !ignored[m] {
			t.Errorf("Middlewares.StatConf.IgnoreContentMethods 缺 %s", m)
		}
	}
}

// 令牌只从环境变量读:仓库里的 yaml 绝不能出现令牌变量名之外的任何令牌痕迹。
func TestEtcYamlCarriesNoAdminToken(t *testing.T) {
	raw, err := os.ReadFile(etcYaml)
	if err != nil {
		t.Fatal(err)
	}
	lower := strings.ToLower(string(raw))
	for _, needle := range []string{"admintoken", "admin_token", "x-mail-admin-token"} {
		if strings.Contains(lower, needle) {
			t.Errorf("%s 里出现了 %q:MailAdmin 令牌只从环境变量 %s 读,不进 yaml / ConfigMap", etcYaml, needle, AdminTokenEnv)
		}
	}
}

// validConfig 是一份能通过 Validate 的最小配置,各拒绝用例在它上面只改一处。
func validConfig() Config {
	var c Config
	c.Name = "mail.rpc"
	c.ListenOn = "127.0.0.1:50900"
	c.Timeout = 4000
	c.Mode = service.DevMode
	c.Etcd.Hosts = []string{"127.0.0.1:2379"}
	c.Redis.Host = "127.0.0.1:6379"
	c.Redis.Type = "node"
	c.ZoneId = 1
	c.LeaseTTL = 60
	c.MySQL = MySQLConf{Host: "127.0.0.1:3306", User: "appuser", DBName: data.DatabaseName, MaxOpenConn: 20, MaxIdleConn: 5}
	c.DataServiceRpc = zrpc.RpcClientConf{}
	c.DataServiceRpc.Etcd.Hosts = []string{"127.0.0.1:2379"}
	c.DataServiceRpc.Etcd.Key = "dataservice.rpc"
	c.IdSegment.Enabled = true
	c.Mail = MailConf{
		MaxPersonalMailsPerPlayer: 100,
		MaxActiveSystemMails:      100,
		MaxPersonalMailTtlDays:    30,
		MaxSystemMailTtlDays:      30,
		AllowedCurrencyTypes:      []uint32{0, 1, 2},
	}
	c.Sweep = SweepConf{Mode: SweepModeReportOnly, Interval: 5 * time.Minute, RetentionDays: 7, BatchLimit: 1000}
	return c
}

func TestValidConfigPasses(t *testing.T) {
	c := validConfig()
	if err := c.Validate(); err != nil {
		t.Fatalf("validConfig 应能通过 Validate: %v", err)
	}
	// Mode=pro 本身也合法:M-9 的闸门在业务层(附件 / ROBOT),不在加载期。
	c.Mode = service.ProMode
	if err := c.Validate(); err != nil {
		t.Fatalf("Mode=pro 应能通过 Validate: %v", err)
	}
}

// 每一条都是"进程能起来、但行为静默出错"的配置,必须在加载期拒掉。
func TestValidateRejects(t *testing.T) {
	cases := []struct {
		name   string
		mutate func(c *Config)
	}{
		{"ZoneId=0", func(c *Config) { c.ZoneId = 0 }},
		{"空 ListenOn", func(c *Config) { c.ListenOn = "" }},
		{"ListenOn 无端口", func(c *Config) { c.ListenOn = "127.0.0.1" }},
		{"ListenOn 端口 0", func(c *Config) { c.ListenOn = "127.0.0.1:0" }},
		{"Timeout 过小", func(c *Config) { c.Timeout = 999 }},
		{"Timeout 过大", func(c *Config) { c.Timeout = 4001 }},
		{"Timeout=0(go-zero 里是不限时)", func(c *Config) { c.Timeout = 0 }},
		{"无 etcd", func(c *Config) { c.Etcd.Hosts = nil }},
		{"Etcd.Key 非空", func(c *Config) { c.Etcd.Key = "mail.rpc" }},
		{"LeaseTTL=0", func(c *Config) { c.LeaseTTL = 0 }},
		{"MySQL 缺 Host", func(c *Config) { c.MySQL.Host = "" }},
		{"MySQL 别的库", func(c *Config) { c.MySQL.DBName = "mmorpg_friend" }},
		{"连接池不限", func(c *Config) { c.MySQL.MaxOpenConn = 0 }},
		{"空闲连接 0", func(c *Config) { c.MySQL.MaxIdleConn = 0 }},
		{"共享 Redis 空", func(c *Config) { c.Redis.Host = "" }},
		{"共享 Redis 集群", func(c *Config) { c.Redis.Type = "cluster" }},
		{"DataServiceRpc 无目标", func(c *Config) { c.DataServiceRpc.Etcd.Key = "" }},
		{"号段未启用", func(c *Config) { c.IdSegment.Enabled = false }},
		{"号段回退 snowflake", func(c *Config) { c.IdSegment.FallbackToSnowflake = true }},
		{"定向上限 0", func(c *Config) { c.Mail.MaxPersonalMailsPerPlayer = 0 }},
		{"定向上限过大", func(c *Config) { c.Mail.MaxPersonalMailsPerPlayer = maxPersonalMailsCeiling + 1 }},
		{"系统上限 0", func(c *Config) { c.Mail.MaxActiveSystemMails = 0 }},
		{"系统上限过大", func(c *Config) { c.Mail.MaxActiveSystemMails = maxActiveSystemMailsCeiling + 1 }},
		{"定向有效期 0", func(c *Config) { c.Mail.MaxPersonalMailTtlDays = 0 }},
		{"系统有效期过大", func(c *Config) { c.Mail.MaxSystemMailTtlDays = maxTtlDaysCeiling + 1 }},
		{"货币白名单缺失", func(c *Config) { c.Mail.AllowedCurrencyTypes = nil }},
		{"货币白名单重复", func(c *Config) { c.Mail.AllowedCurrencyTypes = []uint32{0, 0} }},
		{"货币白名单过长", func(c *Config) { c.Mail.AllowedCurrencyTypes = make([]uint32, maxAllowedCurrencyTypes+1) }},
		{"Sweep 段缺失(Mode 空串)", func(c *Config) { c.Sweep = SweepConf{} }},
		{"Sweep.Mode 非法", func(c *Config) { c.Sweep.Mode = "purge" }},
		{"Sweep.Interval=0", func(c *Config) { c.Sweep.Interval = 0 }},
		{"Sweep.RetentionDays=0", func(c *Config) { c.Sweep.RetentionDays = 0 }},
		{"Sweep.RetentionDays 回绕", func(c *Config) { c.Sweep.RetentionDays = maxRetentionDays + 1 }},
		{"Sweep.BatchLimit=0", func(c *Config) { c.Sweep.BatchLimit = 0 }},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			c := validConfig()
			tc.mutate(&c)
			if err := c.Validate(); err == nil {
				t.Fatalf("%s 应被 Validate 拒绝", tc.name)
			}
		})
	}
}

// 令牌四态:不设 / 去空白后过短 / 恰好够长 / 带首尾空白。短令牌视同未配置(停用),不是"免鉴权"。
func TestResolveAdminToken(t *testing.T) {
	long := strings.Repeat("k", MinAdminTokenBytes)
	cases := []struct {
		name string
		env  map[string]string
		want string
	}{
		{"未设置", map[string]string{}, ""},
		{"空串", map[string]string{AdminTokenEnv: ""}, ""},
		{"只有空白", map[string]string{AdminTokenEnv: "   \t  "}, ""},
		{"过短", map[string]string{AdminTokenEnv: long[:MinAdminTokenBytes-1]}, ""},
		{"过短但带空白凑够长度", map[string]string{AdminTokenEnv: "  " + long[:MinAdminTokenBytes-1] + "  "}, ""},
		{"恰好够长", map[string]string{AdminTokenEnv: long}, long},
		{"首尾空白被去掉", map[string]string{AdminTokenEnv: "\t" + long + "\n"}, long},
		{"别的变量名不算", map[string]string{"MAIL_ADMIN_TOKEN": long}, ""},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			got := ResolveAdminToken(func(k string) string { return tc.env[k] })
			if got != tc.want {
				t.Fatalf("ResolveAdminToken = %d 字节,want %d 字节", len(got), len(tc.want))
			}
		})
	}
}

func TestIsRelaxedMode(t *testing.T) {
	for mode, want := range map[string]bool{
		service.DevMode:  true,
		service.TestMode: true,
		service.ProMode:  false,
		service.RtMode:   false,
		"":               false,
	} {
		if got := IsRelaxedMode(mode); got != want {
			t.Errorf("IsRelaxedMode(%q) = %v, want %v", mode, got, want)
		}
	}
}

func TestRequestBudgetAndAutoMigrate(t *testing.T) {
	c := validConfig()
	if got, want := c.RequestBudget(), 3500*time.Millisecond; got != want {
		t.Errorf("RequestBudget = %v, want %v", got, want)
	}
	if !c.ShouldAutoMigrate() {
		t.Error("Schema 段没写时应默认自动建表")
	}
	off := false
	c.Schema.AutoMigrate = &off
	if c.ShouldAutoMigrate() {
		t.Error("AutoMigrate=false 时应只跑 Plan")
	}
}

func TestAllowsCurrency(t *testing.T) {
	m := MailConf{AllowedCurrencyTypes: []uint32{0, 2}}
	if !m.AllowsCurrency(0) || !m.AllowsCurrency(2) || m.AllowsCurrency(1) || m.AllowsCurrency(3) {
		t.Errorf("AllowsCurrency 与白名单 %v 不一致", m.AllowedCurrencyTypes)
	}
}
