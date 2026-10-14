package config

import (
	"testing"

	"github.com/zeromicro/go-zero/core/conf"
)

// NewPlayerSeed(docs/design/new-player-first-enter-no-cold-read.md)的配置用例,做法同 placement_conf_test.go。

// 块整个不写 = 关:go-zero 不给缺失的 optional 块填内层 default,零值必须就是「不写种子」。
// 反过来如果有人给 Enabled 加 default=true,仓外没写这块的老配置(包括 K8s ConfigMap)会静默开始写种子,
// 而打开前置的两类 go/db 告警可能还没就位(设计 §9 第 2 条)。
func TestNewPlayerSeedBlockMissingMeansOff(t *testing.T) {
	var c Config
	if err := conf.LoadFromYamlBytes([]byte("Name: login.rpc\nListenOn: 127.0.0.1:1\n"), &c); err != nil {
		// 其余必填块缺失会报错;这里只关心 NewPlayerSeed 的零值语义,所以忽略加载错误。
		_ = err
	}
	if c.NewPlayerSeed.Enabled {
		t.Fatal("a missing NewPlayerSeed block must leave Enabled=false")
	}
}

// newPlayerSeedKeysProbe 只用来核对样例 yaml 的段名 / 键名拼写。
//
// 字段**故意不加 optional**:go-zero 的必填语义下,键缺失时 conf.Load 报 "field ... is not set"。
// go-zero 不校验未知键,`NewPlayerSed:` / `Enable:` 这类拼写错误会被静默忽略,而零值恰好就是「关」——
// 只断言零值抓不到拼写错误。json tag 必须与 Config / NewPlayerSeedConf 上的保持一致。
type newPlayerSeedKeysProbe struct {
	NewPlayerSeed struct {
		Enabled bool `json:"Enabled"`
	} `json:"NewPlayerSeed"`
}

// 两份样例 yaml(仓内 etc 与 deploy/login-stack.linux)必须显式写出 NewPlayerSeed.Enabled 且为关
// (设计 §8;打开前置见设计 §7:L-14 对比表、两类 go/db 告警)。
// 键名拼写由 newPlayerSeedKeysProbe 的必填字段保证;「为关」直接读探针的值,不必再加载完整 Config。
func TestLoginYamlSpellsOutNewPlayerSeedKeys(t *testing.T) {
	for _, path := range []string{
		"../../etc/login.yaml",
		"../../../../deploy/login-stack.linux/login.yaml",
	} {
		t.Run(path, func(t *testing.T) {
			var p newPlayerSeedKeysProbe
			if err := conf.Load(path, &p); err != nil {
				t.Fatalf("%s must spell out NewPlayerSeed.Enabled explicitly "+
					"(a typo is silently ignored by go-zero): %v", path, err)
			}
			if p.NewPlayerSeed.Enabled {
				t.Fatalf("%s must keep NewPlayerSeed.Enabled=false until the launch gates are met (design §7)", path)
			}
		})
	}
}

// 探针自身的反例:段名 / 键名写错、整块缺失时必须加载失败,否则上面那条测试是空转。
func TestNewPlayerSeedKeysProbeRejectsTypos(t *testing.T) {
	cases := map[string]string{
		"section typo": "NewPlayerSed:\n  Enabled: false\n",
		"key typo":     "NewPlayerSeed:\n  Enable: false\n",
		"block absent": "Name: login.rpc\n",
	}
	for name, yaml := range cases {
		t.Run(name, func(t *testing.T) {
			var p newPlayerSeedKeysProbe
			if err := conf.LoadFromYamlBytes([]byte(yaml), &p); err == nil {
				t.Fatalf("probe accepted a misspelled / missing NewPlayerSeed block:\n%s", yaml)
			}
		})
	}
	var ok newPlayerSeedKeysProbe
	if err := conf.LoadFromYamlBytes([]byte("NewPlayerSeed:\n  Enabled: false\n"), &ok); err != nil {
		t.Fatalf("probe rejected a correctly spelled NewPlayerSeed block: %v", err)
	}
}

// etc/login.yaml 里的 NewPlayerSeed 必须是关着的(打开前置见设计 §7:L-14 对比表、两类告警)。
// 这里的零值断言**只负责「当前是关着的」**:块缺失或拼错时零值同样成立,
// 键名拼写由 TestLoginYamlSpellsOutNewPlayerSeedKeys 的必填探针保证。
func TestEtcYamlNewPlayerSeedIsOff(t *testing.T) {
	var c Config
	if err := conf.Load("../../etc/login.yaml", &c); err != nil {
		t.Fatalf("load etc/login.yaml: %v", err)
	}
	if c.NewPlayerSeed.Enabled {
		t.Fatal("NewPlayerSeed.Enabled must be false in etc/login.yaml until the launch gates are met")
	}
}
