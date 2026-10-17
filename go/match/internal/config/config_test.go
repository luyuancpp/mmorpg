package config

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/stretchr/testify/require"
	"github.com/zeromicro/go-zero/core/conf"
)

// 仓内 etc/match_service.yaml 必须能加载并通过 Validate,且排行榜快照默认关闭(唯一上线闸门,设计 §5.4)。
func TestMatchServiceYamlLoadsWithSnapshotDefaults(t *testing.T) {
	var c Config
	require.NoError(t, conf.Load(filepath.Join("..", "..", "etc", "match_service.yaml"), &c))
	require.False(t, c.RatingSnapshotPublishEnabled, "仓内默认必须是 false")
	require.Equal(t, "match-rating-snapshot", c.RatingSnapshotTopic)
	require.Equal(t, int32(3), c.RatingSnapshotTopicPartitions)
	require.Equal(t, int64(168), c.RatingSnapshotRetentionHours)
	require.Equal(t, int64(7*24*3600*1000), c.RatingSnapshotRetentionMs())
}

// yaml 里不写快照这一组键时按默认值回填(K8s ConfigMap 漏写也不会启动失败,开关仍是 false)。
func TestSnapshotKeysDefaultWhenAbsent(t *testing.T) {
	src, err := os.ReadFile(filepath.Join("..", "..", "etc", "match_service.yaml"))
	require.NoError(t, err)
	var kept []string
	for _, line := range strings.Split(string(src), "\n") {
		if strings.HasPrefix(strings.TrimSpace(line), "RatingSnapshot") {
			continue
		}
		kept = append(kept, line)
	}
	path := filepath.Join(t.TempDir(), "match_service.yaml")
	require.NoError(t, os.WriteFile(path, []byte(strings.Join(kept, "\n")), 0o600))
	var c Config
	require.NoError(t, conf.Load(path, &c))
	require.False(t, c.RatingSnapshotPublishEnabled)
	require.Equal(t, "match-rating-snapshot", c.RatingSnapshotTopic)
	require.Equal(t, int64(168), c.RatingSnapshotRetentionHours)
}

// conf.Load 会自动调用 Validate(go-zero v1.10.0):yaml 里写坏的快照参数在加载期就被拒,而不是等到 matcher / 消费者都起来之后。
// 只直接调 Validate 的用例证明不了这一点(哪天有人把 Validate 改成值接收者或改了名,加载期就不再校验)。
func TestConfLoadRunsValidate(t *testing.T) {
	src, err := os.ReadFile(filepath.Join("..", "..", "etc", "match_service.yaml"))
	require.NoError(t, err)
	for name, broken := range map[string]string{
		"分区为 0":  "RatingSnapshotTopicPartitions: 0",
		"分区为负":   "RatingSnapshotTopicPartitions: -3",
		"保留期为 0": "RatingSnapshotRetentionHours: 0",
		"保留期为负":  "RatingSnapshotRetentionHours: -1",
	} {
		key := strings.SplitN(broken, ":", 2)[0]
		var lines []string
		replaced := false
		for _, line := range strings.Split(string(src), "\n") {
			if strings.HasPrefix(strings.TrimSpace(line), key+":") {
				line = broken
				replaced = true
			}
			lines = append(lines, line)
		}
		require.True(t, replaced, "夹具前提:仓内 yaml 有 %s 这一行", key)
		path := filepath.Join(t.TempDir(), "match_service.yaml")
		require.NoError(t, os.WriteFile(path, []byte(strings.Join(lines, "\n")), 0o600))
		var c Config
		require.Error(t, conf.Load(path, &c), name)
	}
}

func TestValidateRatingSnapshot(t *testing.T) {
	valid := func() Config {
		c := Config{
			RatingEnabled:                 true,
			RatingSnapshotTopic:           "match-rating-snapshot",
			RatingSnapshotTopicPartitions: 3,
			RatingSnapshotRetentionHours:  168,
		}
		c.Kafka.Brokers = []string{"127.0.0.1:9092"}
		return c
	}
	c := valid()
	require.NoError(t, c.Validate(), "开关关闭时只校验 topic 参数")
	c.RatingSnapshotPublishEnabled = true
	require.NoError(t, c.Validate())

	for name, mutate := range map[string]func(c *Config){
		"topic 为空":        func(c *Config) { c.RatingSnapshotTopic = "" },
		"分区为 0":           func(c *Config) { c.RatingSnapshotTopicPartitions = 0 },
		"保留期为 0":          func(c *Config) { c.RatingSnapshotRetentionHours = 0 },
		"开关开但不消费对局结果":     func(c *Config) { c.RatingSnapshotPublishEnabled = true; c.RatingEnabled = false },
		"开关开但没有 Kafka 地址": func(c *Config) { c.RatingSnapshotPublishEnabled = true; c.Kafka.Brokers = nil },
	} {
		c := valid()
		mutate(&c)
		require.Error(t, c.Validate(), name)
	}

	// 开关关闭时,没有 Kafka 地址 / 不消费对局结果都是合法的本地形态。
	c = valid()
	c.RatingEnabled = false
	c.Kafka.Brokers = nil
	require.NoError(t, c.Validate())
}
