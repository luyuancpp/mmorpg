package task

import (
	"fmt"
	"game/internal/config"
)

func GetQueueName(playerId uint64) string {
	return fmt.Sprintf("shard:%d", playerId%config.AppConfig.ServerConfig.QueueShardCount)
}
