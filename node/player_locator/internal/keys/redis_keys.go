package keys

import "fmt"

const PlayerLocationKeyPrefix = "player_location"

func PlayerLocationKey(uid int64) string {
	return fmt.Sprintf("%s:%d", PlayerLocationKeyPrefix, uid)
}
