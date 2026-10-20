package config

import (
	"reflect"
	"testing"

	"github.com/zeromicro/go-zero/core/conf"
)

// PersistSchemaRegressionMode 的归一化与样例配置用例(设计 §5.1 C4;交接 §3.2 第 3 条的 login 部分)。

func TestParsePersistSchemaRegressionMode(t *testing.T) {
	accepted := map[string]PersistSchemaRegressionMode{
		"":        PersistSchemaRegressionWarn,
		"warn":    PersistSchemaRegressionWarn,
		"enforce": PersistSchemaRegressionEnforce,
	}
	for raw, want := range accepted {
		t.Run("accept "+raw, func(t *testing.T) {
			got, err := ParsePersistSchemaRegressionMode(raw)
			if err != nil || got != want {
				t.Fatalf("ParsePersistSchemaRegressionMode(%q) = (%q, %v), want (%q, nil)", raw, got, err, want)
			}
		})
	}
	// 只认两个字面量:大小写、空白、看起来像开关的值一律拒绝,不猜运维的意图。
	for _, raw := range []string{"WARN", "Enforce", " warn", "enforce ", "off", "disabled", "true", "1", "warn,enforce"} {
		t.Run("reject "+raw, func(t *testing.T) {
			if got, err := ParsePersistSchemaRegressionMode(raw); err == nil {
				t.Fatalf("ParsePersistSchemaRegressionMode(%q) = %q, want an error (login must refuse to start)", raw, got)
			}
		})
	}
}

// Config 上的键必须是 optional:缺键 = 空串 = warn。去掉 optional 会让仓外没写这个键的旧配置
// (K8s ConfigMap、deploy/login-stack.linux)直接起不来。
func TestConfigPersistSchemaRegressionModeTagIsOptional(t *testing.T) {
	f, ok := reflect.TypeOf(Config{}).FieldByName("PersistSchemaRegressionMode")
	if !ok {
		t.Fatal("Config.PersistSchemaRegressionMode is missing")
	}
	if got, want := f.Tag.Get("json"), "PersistSchemaRegressionMode,optional"; got != want {
		t.Fatalf("Config.PersistSchemaRegressionMode json tag = %q, want %q", got, want)
	}
}

// persistSchemaRegressionModeProbe 只用来核对 etc/login.yaml 的键名拼写。
// 字段故意不加 optional:键缺失时 conf.Load 报错。go-zero 不校验未知键,拼错的键会被静默忽略,
// 而空串恰好归一成 warn —— 只断言归一后的值抓不到拼写错误。json tag 必须与 Config 上的键名一致。
type persistSchemaRegressionModeProbe struct {
	PersistSchemaRegressionMode string `json:"PersistSchemaRegressionMode"`
}

// 仓内 etc/login.yaml 必须显式写出这个键,且取值为 warn:切 enforce 的条件(设计 §8.1 第 5 步)满足之前不许改。
func TestEtcYamlSpellsOutPersistSchemaRegressionModeAsWarn(t *testing.T) {
	var p persistSchemaRegressionModeProbe
	if err := conf.Load("../../etc/login.yaml", &p); err != nil {
		t.Fatalf("etc/login.yaml must spell out PersistSchemaRegressionMode explicitly "+
			"(a typo is silently ignored by go-zero): %v", err)
	}
	if p.PersistSchemaRegressionMode != string(PersistSchemaRegressionWarn) {
		t.Fatalf("etc/login.yaml PersistSchemaRegressionMode = %q, want %q until the enforce gate (design §8.1 step 5) is met",
			p.PersistSchemaRegressionMode, PersistSchemaRegressionWarn)
	}
}

// 探针自身的反例:键名写错或缺失时必须加载失败,否则上面那条测试是空转。
func TestPersistSchemaRegressionModeProbeRejectsTypos(t *testing.T) {
	for name, yaml := range map[string]string{
		"key typo":   "PersistSchemaRegresionMode: warn\n",
		"key absent": "Name: login.rpc\n",
	} {
		t.Run(name, func(t *testing.T) {
			var p persistSchemaRegressionModeProbe
			if err := conf.LoadFromYamlBytes([]byte(yaml), &p); err == nil {
				t.Fatalf("probe accepted a misspelled / missing PersistSchemaRegressionMode key:\n%s", yaml)
			}
		})
	}
	var ok persistSchemaRegressionModeProbe
	if err := conf.LoadFromYamlBytes([]byte("PersistSchemaRegressionMode: warn\n"), &ok); err != nil {
		t.Fatalf("probe rejected a correctly spelled key: %v", err)
	}
}
