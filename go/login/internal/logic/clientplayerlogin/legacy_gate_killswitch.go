package clientplayerloginlogic

import login_proto_common "proto/common/base"

// loginPath 是一次 ClientPlayerLogin.Login 调用的入口分类:决定走哪段流程、记哪个指标标签、
// 受不受 LegacyGateLoginEnabled 管。只看会话头与 auth_type,不看认证结果,所以认证之前就能分类。
type loginPath uint8

const (
	// loginPathHTTP:Java Gateway POST /api/login 直调,不带会话。只认证、签 token、回角色列表,
	// 不写 login_session、不登记设备集。
	loginPathHTTP loginPath = iota
	// loginPathGateBind:经 cpp gate 转发、auth_type=access_token。新流程
	// (/api/login → assign-gate → gate 上 Login{access_token})与 access_token 重连的必经一步,
	// 是 login_session 与设备集的**唯一写入点**——CreatePlayer / EnterGame 都靠它找到账号。
	// 它不是弃用路径,kill switch 不能拦它。
	loginPathGateBind
	// loginPathGateLegacy:经 cpp gate 转发、在 gate 上用口令或第三方凭据首认证。
	// ARCH §12 弃用的旧路径,LegacyGateLoginEnabled=false 时拒绝。
	loginPathGateLegacy
)

// classifyLoginPath 按会话头与 auth_type 给 Login 调用分类。
//
// 带会话(SessionId > 0)= 经 cpp gate HandleGrpcNodeMessage 转发;HTTP 入口不带会话。
// gate 上只有 auth_type 恰为 "access_token" 才算会话绑定(与 auth 注册表同样大小写敏感);
// 空 auth_type 是 "password" 的旧拼法,与口令、第三方、未知类型一样归旧路径,开关关闭时 fail-closed。
func classifyLoginPath(sessionDetails *login_proto_common.SessionDetails, sessionFound bool, authType string) loginPath {
	if !sessionFound || sessionDetails == nil || sessionDetails.SessionId == 0 {
		return loginPathHTTP
	}
	if authType == "access_token" {
		return loginPathGateBind
	}
	return loginPathGateLegacy
}

// metricLabel 是 login_auth_path_total 的 path 标签。"new" / "legacy" 沿用上线以来的取值,
// 面板与告警(docs/ops/grafana-login-path-deprecation.json)按它们查询。
func (p loginPath) metricLabel() string {
	switch p {
	case loginPathGateBind:
		return "gate_bind"
	case loginPathGateLegacy:
		return "legacy"
	default:
		return "new"
	}
}

// shouldRejectLegacyRequest 是旧 gate 凭据登录 kill switch 的判定(ARCH §12 T+2)。
//
// 真值表(测试逐格钉住):
//
//	path=HTTP        flagEnabled=any   -> false  (新 HTTP 入口不受开关影响)
//	path=GateBind    flagEnabled=any   -> false  (新流程绑定会话的必经一步,拦了谁都进不了游戏)
//	path=GateLegacy  flagEnabled=true  -> false  (旧路径照常服务,默认状态)
//	path=GateLegacy  flagEnabled=false -> true   (kill switch 生效)
//
// 单独成函数,测试不必搭 LoginLogic / ServiceContext / Redis 就能验证契约。Login() 每次请求调用一次,
// 位于入口分类之后、认证(resolveAccount)之前:被拒的请求不调用认证器、不抢账号锁、不碰 Redis。
func shouldRejectLegacyRequest(path loginPath, flagEnabled bool) bool {
	return path == loginPathGateLegacy && !flagEnabled
}
