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

// PlayerToAccountKey returns the Redis key for the reverse mapping player_id → account.
// Used by rollback orphan cleanup to find which account owns a character.
func PlayerToAccountKey(playerId uint64) string {
	return fmt.Sprintf("player_to_account:%d", playerId)
}
