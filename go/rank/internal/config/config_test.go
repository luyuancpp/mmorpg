package config

import (
	"os"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/conf"
	"github.com/zeromicro/go-zero/core/stores/redis"
)

// validConfig 是一份通过 Validate 的最小配置,各用例在它上面只改一处。
func validConfig() Config {
	var c Config
	c.ListenOn = "127.0.0.1:51100"
	c.Timeout = 4000
	c.ZoneId = 1
	c.LeaseTTL = 60
	c.Etcd.Hosts = []string{"127.0.0.1:2379"}
	c.Redis.RedisConf = redis.RedisConf{Host: "127.0.0.1:6379", Type: "node"}
	c.Kafka.Brokers = []string{"127.0.0.1:9092"}
	c.RatingSnapshotTopic = "match-rating-snapshot"
	c.RatingSnapshotTopicPartitions = 3
	c.RatingConsumerGroup = DefaultRatingConsumerGroup
	c.DataServiceRpc.Etcd.Key = "dataservice.rpc"
	c.BoardSizeSampleInterval = 30 * time.Second
	return c
}

// 仓内 etc/rank.yaml 必须能加载并通过 Validate,关键契约值与设计一致(§5.1 / §5.2 / §2.4)。
func TestRankYamlLoads(t *testing.T) {
	var c Config
	require.NoError(t, conf.Load(filepath.Join("..", "..", "etc", "rank.yaml"), &c))
	require.Equal(t, uint32(1), c.ZoneId)
	require.Equal(t, int64(4000), c.Timeout)
	require.Equal(t, 3500*time.Millisecond, c.RequestBudget())
	require.Equal(t, ":9250", c.MetricsListenAddr, "指标端口 9250(设计 §5.1)")
	require.Equal(t, "", c.Etcd.Key, "全局服务不注册 go-zero key(D-13)")
	require.Equal(t, "match-rating-snapshot", c.RatingSnapshotTopic, "与 match_service.yaml 的同名键同值")
	require.Equal(t, int32(3), c.RatingSnapshotTopicPartitions, "与 match_service.yaml 的同名键同值")
	require.Equal(t, "rank-rating", c.RatingConsumerGroup)
	require.True(t, c.ConsumerEnabled())
	require.True(t, c.RankRedisFallsBack(), "本地 compose 没有独立 RankRedis,回落共享库")
	require.Equal(t, c.Redis.Host, c.EffectiveRankRedis().Host)
	require.Equal(t, 30*time.Second, c.BoardSizeSampleInterval)
	require.Equal(t, "dataservice.rpc", c.DataServiceRpc.Etcd.Key)
}

// 消费组键刻意不叫 GroupID:go_services.ps1 -Zone 会给 GroupID 加 _z<N>,全局 topic 会被各 zone 的 rank 各消费一遍(§5.2)。
func TestRankYamlHasNoGroupIDKey(t *testing.T) {
	src, err := os.ReadFile(filepath.Join("..", "..", "etc", "rank.yaml"))
	require.NoError(t, err)
	for _, line := range strings.Split(string(src), "\n") {
		trimmed := strings.TrimSpace(line)
		if strings.HasPrefix(trimmed, "#") {
			continue
		}
		require.False(t, strings.HasPrefix(trimmed, "GroupID:"), "rank.yaml 不得出现 GroupID 键: %q", line)
	}
}

// conf.Load 会自动调用 Validate(go-zero v1.10.0):yaml 里写坏的值在加载期就被拒。
// 只直接调 Validate 的用例证明不了这一点(哪天有人把 Validate 改成值接收者或改了名,加载期就不再校验)。
func TestConfLoadRunsValidate(t *testing.T) {
	src, err := os.ReadFile(filepath.Join("..", "..", "etc", "rank.yaml"))
	require.NoError(t, err)
	for name, broken := range map[string]string{
		"Timeout 越上限": "Timeout: 4001",
		"ZoneId 为 0":  "ZoneId: 0",
	} {
		key := strings.SplitN(broken, ":", 2)[0]
		var lines []string
		replaced := false
		for _, line := range strings.Split(string(src), "\n") {
			if strings.HasPrefix(line, key+":") {
				line = broken
				replaced = true
			}
			lines = append(lines, line)
		}
		require.True(t, replaced, "夹具前提:仓内 yaml 有顶层 %s 这一行", key)
		path := filepath.Join(t.TempDir(), "rank.yaml")
		require.NoError(t, os.WriteFile(path, []byte(strings.Join(lines, "\n")), 0o600))
		var c Config
		require.Error(t, conf.Load(path, &c), name)
	}
}

func TestValidate(t *testing.T) {
	c := validConfig()
	require.NoError(t, c.Validate())

	for name, mutate := range map[string]func(c *Config){
		"ZoneId 为 0":                  func(c *Config) { c.ZoneId = 0 },
		"ListenOn 为空":                 func(c *Config) { c.ListenOn = "" },
		"ListenOn 没有端口":               func(c *Config) { c.ListenOn = "127.0.0.1" },
		"Timeout 为 0":                 func(c *Config) { c.Timeout = 0 },
		"Timeout 低于下限":                func(c *Config) { c.Timeout = MinRpcTimeoutMs - 1 },
		"Timeout 超过上限":                func(c *Config) { c.Timeout = MaxRpcTimeoutMs + 1 },
		"Etcd.Hosts 为空":               func(c *Config) { c.Etcd.Hosts = nil },
		"Etcd.Key 非空(D-13)":           func(c *Config) { c.Etcd.Key = "rank.rpc" },
		"LeaseTTL 为 0":                func(c *Config) { c.LeaseTTL = 0 },
		"RankRedis 与共享 Redis 都没配":     func(c *Config) { c.Redis.Host = ""; c.RankRedis.Host = "" },
		"消费时 topic 为空":                func(c *Config) { c.RatingSnapshotTopic = "" },
		"消费时分区为 0":                    func(c *Config) { c.RatingSnapshotTopicPartitions = 0 },
		"消费时消费组为空":                    func(c *Config) { c.RatingConsumerGroup = "" },
		"DataServiceRpc 没有目标":         func(c *Config) { c.DataServiceRpc.Etcd.Key = "" },
		"BoardSizeSampleInterval 为 0": func(c *Config) { c.BoardSizeSampleInterval = 0 },
		"Auth=true 但 Redis 不完整(内嵌校验)": func(c *Config) { c.Auth = true; c.Redis.Key = "" },
	} {
		c := validConfig()
		mutate(&c)
		require.Error(t, c.Validate(), name)
	}
}

// 不配 broker = 只读服务(合法):此时 topic / 分区 / 消费组都不校验。
func TestValidateConsumerDisabled(t *testing.T) {
	c := validConfig()
	c.Kafka.Brokers = nil
	c.RatingSnapshotTopic = ""
	c.RatingSnapshotTopicPartitions = 0
	c.RatingConsumerGroup = ""
	require.False(t, c.ConsumerEnabled())
	require.NoError(t, c.Validate())
}

// RankRedis 配了就用它,没配回落共享库(§3.5)。
func TestEffectiveRankRedis(t *testing.T) {
	c := validConfig()
	require.True(t, c.RankRedisFallsBack())
	require.Equal(t, "127.0.0.1:6379", c.EffectiveRankRedis().Host)

	c.RankRedis = redis.RedisConf{Host: "rank-redis:6379", Type: "node"}
	require.False(t, c.RankRedisFallsBack())
	require.Equal(t, "rank-redis:6379", c.EffectiveRankRedis().Host)
	// 只配 RankRedis、不配共享库也合法(rank 不读任何跨运行时契约 key)。
	c.Redis.Host = ""
	require.NoError(t, c.Validate())
}
