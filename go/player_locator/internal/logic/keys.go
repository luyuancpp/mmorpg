package logic

import "fmt"

const (
	locationKeyPrefix = "player:location:"
	sessionKeyPrefix  = "player:session:"
	LeaseZSetKey      = "player:leases"
)

func locationKey(uid int64) string {
	return fmt.Sprintf("%s%d", locationKeyPrefix, uid)
}

func sessionKey(playerID uint64) string {
	return fmt.Sprintf("%s%d", sessionKeyPrefix, playerID)
}
