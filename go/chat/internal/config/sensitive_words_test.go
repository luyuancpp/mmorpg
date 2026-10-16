package config

import (
	"testing"

	"chat/internal/textfilter"

	"github.com/stretchr/testify/require"
)

func TestValidateSensitiveWordsBeforeServiceStartup(t *testing.T) {
	cfg := Config{
		ZoneId: 1, LeaseTTL: 60, MaxContentBytes: 512, RateLimitPerSecond: 5,
		HistoryMaxEntries: 200, HistoryTTLSeconds: 604800, HistoryDefaultLimit: 20,
		HistoryMaxLimit: 50, RequestIdTTLSeconds: 60,
	}
	cfg.ListenOn = "127.0.0.1:50700"
	cfg.Timeout = 3500
	cfg.Etcd.Hosts = []string{"127.0.0.1:2379"}
	cfg.Redis.Host = "127.0.0.1:6379"
	cfg.SensitiveWords = []string{"广告", "Spam", ""}
	require.NoError(t, cfg.Validate())
	cfg.SensitiveWords = make([]string, textfilter.MaxWords+1)
	require.ErrorContains(t, cfg.Validate(), "SensitiveWords")
}
