package config

import "fmt"

// PersistSchemaRegressionMode 是 login 侧持久 schema 世代倒退判定(C4)的模式,
// 设计见 docs/design/mixed-version-persist-field-protection.md §5.1 C4、§8.1 第 5 步。
//
// 只有两个合法取值;运行期一律用 ParsePersistSchemaRegressionMode 的结果,不直接比较原始字符串。
type PersistSchemaRegressionMode string

const (
	// PersistSchemaRegressionWarn:判定倒退时照常 EnterGame,只计数与打日志(引导期默认)。
	PersistSchemaRegressionWarn PersistSchemaRegressionMode = "warn"
	// PersistSchemaRegressionEnforce:判定倒退时本次 EnterGame 失败,走现有失败路径(固定 kEnterSceneFailed)。
	PersistSchemaRegressionEnforce PersistSchemaRegressionMode = "enforce"
)

// ParsePersistSchemaRegressionMode 归一化 Config.PersistSchemaRegressionMode:
//   - 缺键或空串 = warn。go-zero 对 optional 字符串缺键给空串,仓外没写这个键的旧配置
//     (K8s ConfigMap、deploy/login-stack.linux)必须保持引导期的观测行为,不能因此起不来;
//   - "warn" / "enforce" 原样返回;
//   - 其余任何取值(大小写不同、带空白、off 之类)都返回错误,由启动期拒绝启动(login.go)。
//     拼错的 enforce 若静默按 warn 处理,运维以为已切到拒绝,实际仍在放行。
func ParsePersistSchemaRegressionMode(raw string) (PersistSchemaRegressionMode, error) {
	switch PersistSchemaRegressionMode(raw) {
	case "":
		return PersistSchemaRegressionWarn, nil
	case PersistSchemaRegressionWarn, PersistSchemaRegressionEnforce:
		return PersistSchemaRegressionMode(raw), nil
	}
	return "", fmt.Errorf("PersistSchemaRegressionMode=%q 不合法:只接受 warn 或 enforce(缺省/空串按 warn)", raw)
}
