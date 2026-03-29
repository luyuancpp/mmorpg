package ai

import (
	"math/rand"
	"time"

	"go.uber.org/zap"

	"robot/generated/pb/game"
	"robot/metrics"
	"robot/pkg"
	"robot/proto/common/component"
	"robot/proto/scene"
)

// RobotAI drives a single robot's in-game behavior after login+enter.
// It runs in the same goroutine as the robot's recv loop (called between ticks).
type RobotAI struct {
	client    *pkg.GameClient
	stats     *metrics.Stats
	skillIDs  []uint32
	interval  time.Duration
	lastSkill time.Time
}

func NewRobotAI(client *pkg.GameClient, stats *metrics.Stats) *RobotAI {
	return &RobotAI{
		client:   client,
		stats:    stats,
		skillIDs: []uint32{1001}, // default skill table IDs to cycle
		interval: 3 * time.Second,
	}
}

// SetSkillIDs overrides the default skill list.
func (ai *RobotAI) SetSkillIDs(ids []uint32) { ai.skillIDs = ids }

// SetInterval overrides the action interval.
func (ai *RobotAI) SetInterval(d time.Duration) { ai.interval = d }

// RunLoop is a blocking loop that periodically performs actions.
// Call this in its own goroutine alongside the recv loop.
func (ai *RobotAI) RunLoop(stop <-chan struct{}) {
	// Wait a short random delay so robots don't all act at the same instant.
	jitter := time.Duration(rand.Intn(2000)) * time.Millisecond
	time.Sleep(jitter)

	ticker := time.NewTicker(ai.interval)
	defer ticker.Stop()

	for {
		select {
		case <-stop:
			return
		case <-ticker.C:
			ai.doAction()
		}
	}
}

func (ai *RobotAI) doAction() {
	if len(ai.skillIDs) == 0 {
		return
	}

	skillID := ai.skillIDs[rand.Intn(len(ai.skillIDs))]

	// Random position near origin.
	x := rand.Float64()*100 - 50
	z := rand.Float64()*100 - 50

	err := ai.client.SendRequest(game.SceneSkillClientPlayerReleaseSkillMessageId, &scene.ReleaseSkillRequest{
		SkillTableId: skillID,
		Position: &component.Vector3{
			X: x,
			Y: 0,
			Z: z,
		},
	})
	if err != nil {
		zap.L().Warn("send skill failed", zap.Error(err))
		return
	}
	ai.stats.SkillSent()
	ai.stats.MsgSent()
}
