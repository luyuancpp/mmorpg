package metrics

import (
	"fmt"
	"sync"
	"sync/atomic"
	"time"

	"go.uber.org/zap"
)

// Stats tracks aggregate load-test counters.
// All methods are goroutine-safe.
type Stats struct {
	loginOK      atomic.Int64
	loginFail    atomic.Int64
	enterOK      atomic.Int64
	enterFail    atomic.Int64
	msgSent      atomic.Int64
	msgRecv      atomic.Int64
	skillSent    atomic.Int64
	connected    atomic.Int64
	disconnected atomic.Int64

	mu             sync.Mutex
	loginCount     int64         // guarded by mu
	loginTotalNs   int64         // guarded by mu — sum in nanoseconds
	loginMaxNs     int64         // guarded by mu
	reportStart    time.Time
}

func NewStats() *Stats {
	return &Stats{reportStart: time.Now()}
}

func (s *Stats) LoginOK(d time.Duration) {
	s.loginOK.Add(1)
	s.mu.Lock()
	s.loginCount++
	s.loginTotalNs += int64(d)
	if int64(d) > s.loginMaxNs {
		s.loginMaxNs = int64(d)
	}
	s.mu.Unlock()
}
func (s *Stats) LoginFail()    { s.loginFail.Add(1) }
func (s *Stats) EnterOK()      { s.enterOK.Add(1) }
func (s *Stats) EnterFail()    { s.enterFail.Add(1) }
func (s *Stats) MsgSent()      { s.msgSent.Add(1) }
func (s *Stats) MsgRecv()      { s.msgRecv.Add(1) }
func (s *Stats) SkillSent()    { s.skillSent.Add(1) }
func (s *Stats) Connected()    { s.connected.Add(1) }
func (s *Stats) Disconnected() { s.disconnected.Add(1) }

// StartReporter prints a summary every interval until stop is closed.
func (s *Stats) StartReporter(interval time.Duration, stop <-chan struct{}) {
	go func() {
		ticker := time.NewTicker(interval)
		defer ticker.Stop()
		for {
			select {
			case <-ticker.C:
				s.report()
			case <-stop:
				s.report()
				return
			}
		}
	}()
}

func (s *Stats) report() {
	elapsed := time.Since(s.reportStart).Truncate(time.Second)

	s.mu.Lock()
	count := s.loginCount
	totalNs := s.loginTotalNs
	maxNs := s.loginMaxNs
	s.mu.Unlock()

	var avg, max time.Duration
	if count > 0 {
		avg = time.Duration(totalNs / count)
		max = time.Duration(maxNs)
	}

	zap.L().Info(fmt.Sprintf("[stats %s] conn=%d login_ok=%d login_fail=%d enter_ok=%d enter_fail=%d "+
		"msg_sent=%d msg_recv=%d skill=%d avg_login=%s max_login=%s",
		elapsed,
		s.connected.Load(),
		s.loginOK.Load(), s.loginFail.Load(),
		s.enterOK.Load(), s.enterFail.Load(),
		s.msgSent.Load(), s.msgRecv.Load(),
		s.skillSent.Load(),
		avg.Truncate(time.Millisecond),
		max.Truncate(time.Millisecond),
	))
}
