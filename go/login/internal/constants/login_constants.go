package constants

import "fmt"

const (
	LoginAccountSessionsKey   = "login_account_sessions"
	RedisKeyPrefixAccountData = "account"
)

func GenerateSessionKey(account string) string {
	return fmt.Sprintf("%s:%s", LoginAccountSessionsKey, account)
}

func GetAccountDataKey(account string) string {
	return fmt.Sprintf("%s:%s", RedisKeyPrefixAccountData, account)
}

func GetPlayerDataKey(playerId uint64) string {
	return fmt.Sprintf("player_data:%d", playerId)
}
