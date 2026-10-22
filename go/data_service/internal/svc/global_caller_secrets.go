package svc

import (
	"fmt"
	"shared/accountdirectory"
	"shared/serviceauth"
	"strings"
)

// 独立caller密钥必须互异；共享同一HMAC值会让只读SM伪装login/scene/trade，purpose本身挡不住此种配置。
func validateGlobalCallerSecrets(getenv func(string) string) error {
	seen := map[string]bool{}
	for _, entry := range []struct{ caller, env string }{{"login", accountdirectory.SecretEnv}, {"trade", accountdirectory.TradeSecretEnv}, {"scene", accountdirectory.SceneSecretEnv}, {"scene_manager", "MMORPG_ACCOUNT_DIRECTORY_SECRET_SCENE_MANAGER"}} {
		secret := strings.TrimSpace(getenv(entry.env))
		if _, err := serviceauth.New(map[string]string{entry.caller: secret}); err != nil {
			return fmt.Errorf("全球目录caller %s独立密钥缺失或非法", entry.caller)
		}
		if seen[secret] {
			return fmt.Errorf("全球目录caller密钥不能跨用途复用")
		}
		seen[secret] = true
	}
	return nil
}
