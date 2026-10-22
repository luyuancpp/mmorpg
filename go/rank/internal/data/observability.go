package data

import (
	"errors"

	"rank/internal/metrics"
)

// 只在外层SQL API返回后记最终结果；事务内的尝试/回滚不得提前计ok。
func observeWrite(operation string, err error, replay bool) {
	metrics.ObserveWrite(operation, writeOutcome(err, replay))
}

func writeOutcome(err error, replay bool) string {
	outcome := metrics.WriteOK
	switch {
	case err == nil && replay:
		outcome = metrics.WriteReplay
	case err == nil:
	case errors.Is(err, ErrInvalid), errors.Is(err, ErrDenied), errors.Is(err, ErrConflict), errors.Is(err, ErrCursor), errors.Is(err, ErrClosed), errors.Is(err, ErrNotFound):
		outcome = metrics.WriteRejected
	default:
		outcome = metrics.WriteError
	}
	return outcome
}
