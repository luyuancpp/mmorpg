package authority

import (
	"context"
	"os"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
	"match/internal/config"
)

func policyFixture(t *testing.T) (*Store, config.Config) {
	t.Helper()
	if os.Getenv("MATCH_IT_REQUIRE_TIDB") == "1" && os.Getenv("MATCH_IT_TIDB") == "" {
		t.Fatal("缺少MATCH_IT_TIDB，策略准入回归不得跳过")
	}
	s := testStore(t)
	c := config.Config{GlobalMatch: config.GlobalMatchConf{CrossRegionWaitSeconds: 30, HardLatencyMs: 180, ProbeTTLSeconds: 120},
		RatingEnabled: true, RatingToleranceBase: 100, RatingToleranceStepSeconds: 5, RatingToleranceStepDelta: 100,
		RatingToleranceMax: 1000, RatingToleranceMaxWaitSeconds: 90, ChallengeTTLSeconds: 60,
		PveTeamSizeByConfigId: map[string]uint32{"1": 2, "7": 3}}
	return s, c
}

func TestAuthorityTiDBGlobalPolicyCannotBeOverwrittenByWaitHardLimitSecretOrRules(t *testing.T) {
	s, original := policyFixture(t)
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	require.NoError(t, s.EnsureGlobalPolicy(ctx, original))
	var before []byte
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT contract_digest FROM match_global_policy WHERE policy_id=1").Scan(&before))
	for _, mode := range []string{"wait", "hard", "secret", "ttl", "rating", "challenge", "pve-team-size", "team-cross-zone", "team-fill", "entry-guild-schema", "entry-role-schema"} {
		t.Run(mode, func(t *testing.T) {
			copyStore := *s
			changed := original
			switch mode {
			case "entry-guild-schema":
				changed.GlobalMatch.GuildTrialSchema = "another_guild"
			case "entry-role-schema":
				changed.GlobalMatch.AccountRoleSchema = "another_role"
			case "team-cross-zone":
				changed.Team.AllowCrossZone = !original.Team.AllowCrossZone
			case "team-fill":
				changed.Team.FillQueueDisabled = !original.Team.FillQueueDisabled
			case "wait":
				changed.GlobalMatch.CrossRegionWaitSeconds++
			case "hard":
				changed.GlobalMatch.HardLatencyMs++
			case "secret":
				copyStore.Secret = []byte("another-fixture-only-policy-secret-00001")
			case "ttl":
				changed.GlobalMatch.ProbeTTLSeconds++
			case "rating":
				changed.RatingToleranceBase++
			case "challenge":
				changed.ChallengeTTLSeconds++
			case "pve-team-size":
				changed.PveTeamSizeByConfigId = map[string]uint32{"1": 2, "7": 4}
			}
			require.Error(t, copyStore.EnsureGlobalPolicy(ctx, changed))
			var after []byte
			require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT contract_digest FROM match_global_policy WHERE policy_id=1").Scan(&after))
			require.Equal(t, before, after)
			require.NoError(t, s.EnsureGlobalPolicy(ctx, original))
		})
	}
	// map的构造顺序不属于业务策略；同一份有效规则在新副本仍应准入。
	reordered := original
	reordered.PveTeamSizeByConfigId = map[string]uint32{"7": 3, "1": 2}
	require.NoError(t, s.EnsureGlobalPolicy(ctx, reordered))
}

func TestAuthorityTiDBConflictingInitialPoliciesHaveOnePermanentWinner(t *testing.T) {
	s, first := policyFixture(t)
	ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer cancel()
	second := first
	second.GlobalMatch.CrossRegionWaitSeconds++
	start := make(chan struct{})
	out := make(chan struct {
		index int
		err   error
	}, 2)
	for index, candidate := range []config.Config{first, second} {
		go func(index int, candidate config.Config) {
			<-start
			out <- struct {
				index int
				err   error
			}{index, s.EnsureGlobalPolicy(ctx, candidate)}
		}(index, candidate)
	}
	close(start)
	winner := -1
	for range 2 {
		select {
		case result := <-out:
			if result.err == nil {
				require.Equal(t, -1, winner)
				winner = result.index
			} else {
				require.ErrorContains(t, result.err, "策略不一致")
			}
		case <-ctx.Done():
			t.Fatal("策略首次准入并发超预算")
		}
	}
	require.NotEqual(t, -1, winner)
	policies := []config.Config{first, second}
	require.NoError(t, s.EnsureGlobalPolicy(ctx, policies[winner]))
	require.Error(t, s.EnsureGlobalPolicy(ctx, policies[1-winner]))
	var count int
	require.NoError(t, s.DB.QueryRowContext(ctx, "SELECT COUNT(*) FROM match_global_policy").Scan(&count))
	require.Equal(t, 1, count)
}
