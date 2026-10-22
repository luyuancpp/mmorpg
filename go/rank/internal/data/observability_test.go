package data

import (
	"context"
	"errors"
	"fmt"
	"testing"

	"rank/internal/metrics"
)

func TestWriteObservationSeparatesReplayRejectedAndUnknownCommit(t *testing.T) {
	for _, test := range []struct {
		name   string
		err    error
		replay bool
		want   string
	}{
		{name: "确认提交", want: metrics.WriteOK},
		{name: "原回执重放", replay: true, want: metrics.WriteReplay},
		{name: "业务拒绝", err: fmt.Errorf("cursor: %w", ErrCursor), want: metrics.WriteRejected},
		{name: "提交响应丢失不能算成功或回放", err: errors.New("commit: connection reset"), replay: true, want: metrics.WriteError},
		{name: "上下文截止保留未知", err: context.DeadlineExceeded, want: metrics.WriteError},
	} {
		t.Run(test.name, func(t *testing.T) {
			if actual := writeOutcome(test.err, test.replay); actual != test.want {
				t.Fatalf("got %q want %q", actual, test.want)
			}
		})
	}
}
