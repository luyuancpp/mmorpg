package constants

import "fmt"

const (
	// 登录会话相关的 Redis 键前缀
	LoginAccountSessionsKey   = "login_account_sessions"
	RedisKeyPrefixAccountData = "account"
)

// 生成 sessionKey 的函数
func GenerateSessionKey(account string) string {
	return fmt.Sprintf("%s:%s", LoginAccountSessionsKey, account)
}

func GetAccountDataKey(account string) string {
	return fmt.Sprintf("%s:%s", RedisKeyPrefixAccountData, account)
}

func GetPlayerDataKey(playerId uint64) string {
	return fmt.Sprintf("player_data:%d", playerId)
}
