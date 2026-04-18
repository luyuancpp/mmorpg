package metrics

import (
	"path/filepath"
	"testing"
	"time"
)

func TestExportBehaviorCSV(t *testing.T) {
	s := NewStats()
	s.RecordBehavior(BehaviorRecord{
		Timestamp: time.Unix(0, 0),
		Account:   "robot_0001",
		Scenario:  "SkillCast",
		Action:    "cast_skill",
		Success:   true,
		LatencyMs: 12,
	})

	out := filepath.Join(t.TempDir(), "behavior.csv")
	if err := s.ExportBehaviorCSV(out); err != nil {
		t.Fatalf("ExportBehaviorCSV failed: %v", err)
	}
}
