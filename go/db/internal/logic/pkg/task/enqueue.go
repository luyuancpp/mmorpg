package task

import (
	"db/internal/config"
	"fmt"
)

func GetQueueName(playerId uint64) string {
	return fmt.Sprintf("shard:%d", playerId%config.AppConfig.ServerConfig.QueueShardCount)
}
