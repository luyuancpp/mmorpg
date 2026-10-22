package svc

import (
	"context"
	"errors"
	"testing"

	"github.com/stretchr/testify/require"
)

type bootstrapFixture struct {
	routes, history bool
	failure         error
	patterns        []string
	redisKeys       []string
}

func (f *bootstrapFixture) HasAnyRoute(context.Context) (bool, error) { return f.routes, f.failure }
func (f *bootstrapFixture) HasHistoricalPlayers(context.Context, []uint32) (bool, error) {
	return f.history, f.failure
}
func (f *bootstrapFixture) ScanCtx(ctx context.Context, cursor uint64, pattern string, _ int64) ([]string, uint64, error) {
	f.patterns = append(f.patterns, pattern)
	return f.redisKeys, 0, ctx.Err()
}

func TestGlobalEmptyRouteBootstrapRequiresEmptySQLHistoryAndCache(t *testing.T) {
	for _, mode := range []string{"new-empty", "known-route", "historical-directory-or-player", "historical-cache", "sql-unknown", "cancelled"} {
		t.Run(mode, func(t *testing.T) {
			f := &bootstrapFixture{}
			ctx, cancel := context.WithCancel(context.Background())
			defer cancel()
			switch mode {
			case "known-route":
				f.routes = true
			case "historical-directory-or-player":
				f.history = true
			case "historical-cache":
				f.redisKeys = []string{"player:zone:11"}
			case "sql-unknown":
				f.failure = errors.New("SQL unknown")
			case "cancelled":
				cancel()
			}
			err := checkEmptyRouteBootstrap(ctx, f, f, []uint32{1})
			if mode == "new-empty" {
				require.NoError(t, err)
				require.Len(t, f.patterns, 4)
			} else if mode == "known-route" {
				require.NoError(t, err)
				require.Empty(t, f.patterns)
			} else {
				require.Error(t, err)
			}
		})
	}
}
