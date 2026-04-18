package metrics

import (
	"encoding/csv"
	"encoding/json"
	"fmt"
	"os"
	"strconv"
	"sync"
	"sync/atomic"
	"time"

	"go.uber.org/zap"
)

// Stats tracks aggregate load-test counters.
// All methods are goroutine-safe.
type Stats struct {
	loginOK         atomic.Int64
	loginFail       atomic.Int64
	loginStuck      atomic.Int64
	enterOK         atomic.Int64
	enterFail       atomic.Int64
	msgSent         atomic.Int64
	msgRecv         atomic.Int64
	skillSent       atomic.Int64
	sceneSwitchSent atomic.Int64
	connected       atomic.Int64
	disconnected    atomic.Int64

	mu             sync.Mutex
	loginCount     int64 // guarded by mu
	loginTotalNs   int64 // guarded by mu — sum in nanoseconds
	loginMaxNs     int64 // guarded by mu
	reportStart    time.Time
	prevMsgSent    int64     // guarded by mu — snapshot for rate calc
	prevMsgRecv    int64     // guarded by mu
	prevReportTime time.Time // guarded by mu

	loginRecords    []LoginRecord    // guarded by mu — per-attempt records for analysis
	behaviorRecords []BehaviorRecord // guarded by mu — per-action records for LLM/ML analysis
}

func NewStats() *Stats {
	now := time.Now()
	return &Stats{reportStart: now, prevReportTime: now}
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
func (s *Stats) LoginFail()       { s.loginFail.Add(1) }
func (s *Stats) LoginStuck()      { s.loginStuck.Add(1) }
func (s *Stats) EnterOK()         { s.enterOK.Add(1) }
func (s *Stats) EnterFail()       { s.enterFail.Add(1) }
func (s *Stats) MsgSent()         { s.msgSent.Add(1) }
func (s *Stats) MsgRecv()         { s.msgRecv.Add(1) }
func (s *Stats) SkillSent()       { s.skillSent.Add(1) }
func (s *Stats) SceneSwitchSent() { s.sceneSwitchSent.Add(1) }
func (s *Stats) Connected()       { s.connected.Add(1) }
func (s *Stats) Disconnected()    { s.disconnected.Add(1) }

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
	now := time.Now()
	elapsed := now.Sub(s.reportStart).Truncate(time.Second)

	s.mu.Lock()
	count := s.loginCount
	totalNs := s.loginTotalNs
	maxNs := s.loginMaxNs
	prevSent := s.prevMsgSent
	prevRecv := s.prevMsgRecv
	prevTime := s.prevReportTime
	s.mu.Unlock()

	curSent := s.msgSent.Load()
	curRecv := s.msgRecv.Load()

	var sendRate, recvRate float64
	if dt := now.Sub(prevTime).Seconds(); dt > 0 {
		sendRate = float64(curSent-prevSent) / dt
		recvRate = float64(curRecv-prevRecv) / dt
	}

	s.mu.Lock()
	s.prevMsgSent = curSent
	s.prevMsgRecv = curRecv
	s.prevReportTime = now
	s.mu.Unlock()

	var avg, max time.Duration
	if count > 0 {
		avg = time.Duration(totalNs / count)
		max = time.Duration(maxNs)
	}

	zap.L().Info(fmt.Sprintf("[stats %s] conn=%d login_ok=%d login_fail=%d login_stuck=%d enter_ok=%d enter_fail=%d "+
		"msg_sent=%d(%.0f/s) msg_recv=%d(%.0f/s) skill=%d scene_switch=%d avg_login=%s max_login=%s",
		elapsed,
		s.connected.Load(),
		s.loginOK.Load(), s.loginFail.Load(), s.loginStuck.Load(),
		s.enterOK.Load(), s.enterFail.Load(),
		curSent, sendRate, curRecv, recvRate,
		s.skillSent.Load(),
		s.sceneSwitchSent.Load(),
		avg.Truncate(time.Millisecond),
		max.Truncate(time.Millisecond),
	))
}

// LoginRecord captures one login attempt for post-hoc analysis / ML training.
type LoginRecord struct {
	Timestamp  time.Time
	Account    string
	Scenario   string // test scenario name or "stress"
	LatencyMs  int64
	Success    bool
	Stuck      bool // true if the attempt timed out waiting for a response
	ErrorMsg   string
	Attempt    int // retry attempt number (1-based)
	Concurrent int // number of concurrent connections at the time
}

// BehaviorRecord captures robot action traces for gameplay-pressure and LLM/ML analysis.
type BehaviorRecord struct {
	Timestamp  time.Time
	Account    string
	Scenario   string
	Action     string
	Success    bool
	LatencyMs  int64
	Detail     string
	SceneID    uint64
	Concurrent int
}

// RecordLogin appends a login attempt record (goroutine-safe).
func (s *Stats) RecordLogin(r LoginRecord) {
	s.mu.Lock()
	s.loginRecords = append(s.loginRecords, r)
	s.mu.Unlock()
}

func (s *Stats) RecordBehavior(r BehaviorRecord) {
	s.mu.Lock()
	s.behaviorRecords = append(s.behaviorRecords, r)
	s.mu.Unlock()
}

func (s *Stats) BehaviorRecordCount() int {
	s.mu.Lock()
	defer s.mu.Unlock()
	return len(s.behaviorRecords)
}

// ExportLoginCSV writes all recorded login attempts to a CSV file for ML analysis.
// Columns: timestamp, account, scenario, latency_ms, success, stuck, error, attempt, concurrent
func (s *Stats) ExportLoginCSV(path string) error {
	s.mu.Lock()
	records := make([]LoginRecord, len(s.loginRecords))
	copy(records, s.loginRecords)
	s.mu.Unlock()

	if len(records) == 0 {
		zap.L().Info("no login records to export")
		return nil
	}

	f, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("create csv: %w", err)
	}
	defer f.Close()

	w := csv.NewWriter(f)
	defer w.Flush()

	_ = w.Write([]string{
		"timestamp", "account", "scenario", "latency_ms",
		"success", "stuck", "error", "attempt", "concurrent",
	})

	for _, r := range records {
		_ = w.Write([]string{
			r.Timestamp.Format(time.RFC3339Nano),
			r.Account,
			r.Scenario,
			strconv.FormatInt(r.LatencyMs, 10),
			strconv.FormatBool(r.Success),
			strconv.FormatBool(r.Stuck),
			r.ErrorMsg,
			strconv.Itoa(r.Attempt),
			strconv.Itoa(r.Concurrent),
		})
	}

	zap.L().Info("exported login records", zap.String("path", path), zap.Int("count", len(records)))
	return nil
}

func (s *Stats) ExportBehaviorCSV(path string) error {
	s.mu.Lock()
	records := make([]BehaviorRecord, len(s.behaviorRecords))
	copy(records, s.behaviorRecords)
	s.mu.Unlock()

	if len(records) == 0 {
		zap.L().Info("no behavior records to export")
		return nil
	}

	f, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("create behavior csv: %w", err)
	}
	defer f.Close()

	w := csv.NewWriter(f)
	defer w.Flush()

	_ = w.Write([]string{
		"timestamp", "account", "scenario", "action", "success",
		"latency_ms", "detail", "scene_id", "concurrent",
	})

	for _, r := range records {
		_ = w.Write([]string{
			r.Timestamp.Format(time.RFC3339Nano),
			r.Account,
			r.Scenario,
			r.Action,
			strconv.FormatBool(r.Success),
			strconv.FormatInt(r.LatencyMs, 10),
			r.Detail,
			strconv.FormatUint(r.SceneID, 10),
			strconv.Itoa(r.Concurrent),
		})
	}

	zap.L().Info("exported behavior records", zap.String("path", path), zap.Int("count", len(records)))
	return nil
}

func (s *Stats) ExportBehaviorJSONL(path string) error {
	s.mu.Lock()
	records := make([]BehaviorRecord, len(s.behaviorRecords))
	copy(records, s.behaviorRecords)
	s.mu.Unlock()

	if len(records) == 0 {
		zap.L().Info("no behavior records to export as jsonl")
		return nil
	}

	f, err := os.Create(path)
	if err != nil {
		return fmt.Errorf("create behavior jsonl: %w", err)
	}
	defer f.Close()

	enc := json.NewEncoder(f)
	for _, r := range records {
		if err := enc.Encode(r); err != nil {
			return fmt.Errorf("encode behavior jsonl: %w", err)
		}
	}

	zap.L().Info("exported behavior jsonl", zap.String("path", path), zap.Int("count", len(records)))
	return nil
}
