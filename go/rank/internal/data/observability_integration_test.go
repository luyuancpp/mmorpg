package data

import (
	"context"
	"testing"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	mailpb "proto/mail"
	rankpb "proto/rank"
	"rank/internal/metrics"
)

func observedRankMetric(t *testing.T, name string, labels map[string]string) float64 {
	t.Helper()
	families, err := prometheus.DefaultGatherer.Gather()
	if err != nil {
		t.Fatal(err)
	}
	for _, family := range families {
		if family.GetName() != name {
			continue
		}
		for _, metric := range family.Metric {
			matched := len(metric.Label) == len(labels)
			for _, label := range metric.Label {
				matched = matched && labels[label.GetName()] == label.GetValue()
			}
			if matched {
				if metric.Counter != nil {
					return metric.Counter.GetValue()
				}
				return metric.Gauge.GetValue()
			}
		}
	}
	t.Fatalf("指标不存在: %s %v", name, labels)
	return 0
}

func TestTiDBRankMetricsRequireCommitAndRetainDueDelayOnReadFailure(t *testing.T) {
	ctx, db, next := rankFixture(t)
	metrics.Register()
	r := New(db, next, next, func(context.Context) (uint64, error) { return 999, nil }, func(uint32) bool { return true })
	r.now = func() time.Time { return time.UnixMilli(2000000) }
	board := testOpen(t, ctx, r)
	_, err := r.Submit(ctx, "match", &rankpb.SubmitRankScoresRequest{BoardId: board.BoardId, Generation: 1, SourceEpoch: 11, SourceCursor: 1, RequestId: 10, Mode: rankpb.RankSubmitMode_RANK_SUBMIT_SET, Entries: []*rankpb.RankScoreInput{{EntityId: 7, Score: 100, AchievedMs: 1}, {EntityId: 8, Score: 90, AchievedMs: 1}}})
	if err != nil {
		t.Fatal(err)
	}
	freeze := &rankpb.FreezeRankGenerationRequest{BoardId: board.BoardId, Generation: 1, SourceEpoch: 11, ExpectedSourceCursor: 1, RequestId: 20, TopN: 2, Rewards: []*rankpb.RankRewardTier{{FirstRank: 1, LastRank: 2, Attachments: &mailpb.MailAttachments{Currencies: []*mailpb.MailCurrency{{Amount: 10}}}}}}
	writeValue := func(outcome string) float64 {
		return observedRankMetric(t, "rank_write_total", map[string]string{"operation": metrics.OperationFreeze, "outcome": outcome})
	}
	beforeOK, beforeError, beforeReplay := writeValue(metrics.WriteOK), writeValue(metrics.WriteError), writeValue(metrics.WriteReplay)
	if _, err = r.Freeze(ctx, "match", freeze); err == nil {
		t.Fatal("重复奖励ID必须回滚")
	}
	if writeValue(metrics.WriteOK) != beforeOK || writeValue(metrics.WriteReplay) != beforeReplay || writeValue(metrics.WriteError) != beforeError+1 {
		t.Fatal("回滚不得记录成功或回放")
	}
	r.rewardIDs = next
	if _, err = r.Freeze(ctx, "match", freeze); err != nil {
		t.Fatal(err)
	}
	if _, err = r.Freeze(ctx, "match", freeze); err != nil {
		t.Fatal(err)
	}
	if writeValue(metrics.WriteOK) != beforeOK+1 || writeValue(metrics.WriteReplay) != beforeReplay+1 {
		t.Fatal("确认提交和原回执必须分开")
	}
	ids, err := r.DueRewards(ctx, 2007500, 100)
	if err != nil || len(ids) != 2 || observedRankMetric(t, "rank_reward_due_delay_seconds", nil) != 7.5 {
		t.Fatalf("真实持久奖励到期延迟: %v %v", ids, err)
	}
	failed, cancel := context.WithCancel(ctx)
	cancel()
	if _, err = r.DueRewards(failed, 2010000, 100); err == nil {
		t.Fatal("取消的SQL查询必须失败")
	}
	if observedRankMetric(t, "rank_reward_due_delay_seconds", nil) != 7.5 {
		t.Fatal("查询失败不得假报空队列")
	}
	if ids, err = r.DueRewards(ctx, 1999999, 100); err != nil || len(ids) != 0 || observedRankMetric(t, "rank_reward_due_delay_seconds", nil) != 0 {
		t.Fatalf("真实空查询才清0: %v %v", ids, err)
	}
}
