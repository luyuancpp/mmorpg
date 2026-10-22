package authority

import (
	"bytes"
	"testing"

	"github.com/stretchr/testify/require"
)

func TestPolicyPinsEveryEffectiveRuleAndWordBoundary(t *testing.T) {
	original := fixtureRules()
	before := policyBytes(original)
	for _, change := range []func(*Rules){func(r *Rules) { r.MaxContentBytes++ }, func(r *Rules) { r.RateLimitPerSecond++ }, func(r *Rules) { r.HistoryMaxEntries++ }, func(r *Rules) { r.HistoryTTLSeconds++ }, func(r *Rules) { r.HistoryDefaultLimit++ }, func(r *Rules) { r.HistoryMaxLimit++ }, func(r *Rules) { r.SensitiveWords = []string{"广", "告"} }} {
		changed := original
		change(&changed)
		require.False(t, bytes.Equal(before, policyBytes(changed)))
	}
	left, right := original, original
	left.SensitiveWords = []string{"a", "bc"}
	right.SensitiveWords = []string{"ab", "c"}
	require.NotEqual(t, policyBytes(left), policyBytes(right))
	require.Equal(t, before, policyBytes(original), "编码不得改变调用者原规则")
}
