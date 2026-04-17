package ai

import (
	"fmt"
	"math/rand"
	"time"

	"go.uber.org/zap"

	"proto/chat"
	"proto/common/component"
	"proto/scene"
	"robot/generated/pb/game"
	"robot/logic/gameobject"
	"robot/metrics"
	"robot/pkg"
)

// RobotAI drives a single robot's in-game behavior after login+enter.
type RobotAI struct {
	client   *pkg.GameClient
	stats    *metrics.Stats
	profile  *Profile
	llm      *LLMAdvisor // nil = use profile weights only
	player   *gameobject.Player
	skillIDs []uint32 // fallback from config if GetSkillList not yet received
	interval time.Duration
	tick     int
	posX     float64
	posZ     float64
}

func NewRobotAI(client *pkg.GameClient, stats *metrics.Stats) *RobotAI {
	p := BuiltinProfiles["stress"] // backward-compatible default: skill-only
	return &RobotAI{
		client:   client,
		stats:    stats,
		profile:  &p,
		interval: 3 * time.Second,
	}
}

func (ai *RobotAI) SetSkillIDs(ids []uint32)    { ai.skillIDs = ids }
func (ai *RobotAI) SetInterval(d time.Duration)  { ai.interval = d }
func (ai *RobotAI) SetProfile(p *Profile)         { ai.profile = p }
func (ai *RobotAI) SetLLM(llm *LLMAdvisor)        { ai.llm = llm }
func (ai *RobotAI) SetPlayer(p *gameobject.Player) { ai.player = p }

// RunLoop is a blocking loop that periodically performs actions.
func (ai *RobotAI) RunLoop(stop <-chan struct{}) {
	jitter := time.Duration(rand.Intn(2000)) * time.Millisecond
	time.Sleep(jitter)

	ticker := time.NewTicker(ai.interval)
	defer ticker.Stop()

	for {
		select {
		case <-stop:
			return
		case <-ticker.C:
			ai.tick++
			ai.doAction()
		}
	}
}

func (ai *RobotAI) doAction() {
	action := ai.pickAction()

	switch action {
	case ActionCastSkill:
		ai.castSkill()
	case ActionMove:
		ai.move()
	case ActionChat:
		ai.sendChat()
	case ActionIdle:
		// do nothing
	}
}

// pickAction decides what to do next — either via LLM or weighted random.
func (ai *RobotAI) pickAction() Action {
	if ai.llm != nil {
		state := GameState{
			PlayerId:  ai.client.PlayerId,
			PosX:      ai.posX,
			PosZ:      ai.posZ,
			SkillIDs:  ai.skillIDs,
			TickCount: ai.tick,
		}
		action, reason := ai.llm.DecideAction(state)
		zap.L().Debug("llm decision",
			zap.String("action", action.String()),
			zap.String("reason", reason),
		)
		return action
	}
	return ai.profile.Pick()
}

// --- Action implementations (flat, no deep call chain) ---

func (ai *RobotAI) castSkill() {
	// Prefer server-reported owned skills, fall back to config/table skills.
	skillIDs := ai.skillIDs
	if ai.player != nil {
		if owned := ai.player.GetOwnedSkillIDs(); len(owned) > 0 {
			skillIDs = owned
		}
	}
	if len(skillIDs) == 0 {
		return
	}

	skillID := skillIDs[rand.Intn(len(skillIDs))]
	x := ai.posX + rand.Float64()*20 - 10
	z := ai.posZ + rand.Float64()*20 - 10

	// Pick a target entity (self or nearby). Server requires target_id > 0.
	var targetID uint64
	if ai.player != nil {
		targetID = ai.player.GetRandomEntity()
	}
	if targetID == 0 {
		return // no known entities yet, skip this tick
	}

	err := ai.client.SendRequest(game.SceneSkillClientPlayerReleaseSkillMessageId, &scene.ReleaseSkillRequest{
		SkillTableId: skillID,
		TargetId:     targetID,
		Position:     &component.Vector3{X: x, Y: 0, Z: z},
	})
	if err != nil {
		zap.L().Warn("cast skill failed", zap.Error(err))
		return
	}
	ai.stats.SkillSent()
	ai.stats.MsgSent()
}

func (ai *RobotAI) move() {
	// Walk to a random nearby position.
	dx := rand.Float64()*30 - 15
	dz := rand.Float64()*30 - 15
	ai.posX += dx
	ai.posZ += dz

	// Use SceneInfoC2S to report new position (if the server supports it).
	err := ai.client.SendRequest(game.SceneSceneClientPlayerSceneInfoC2SMessageId, &scene.SceneInfoRequest{})
	if err != nil {
		zap.L().Warn("move failed", zap.Error(err))
		return
	}
	ai.stats.MsgSent()
}

func (ai *RobotAI) sendChat() {
	messages := []string{
		"hello!", "anyone here?", "gg", "let's go", "lol",
		"need heal", "follow me", "good fight", "afk",
	}
	msg := messages[rand.Intn(len(messages))]

	err := ai.client.SendRequest(game.ClientPlayerChatSendChatMessageId, &chat.SendChatRequest{
		Message: &chat.ChatMessage{
			Content: fmt.Sprintf("[robot] %s", msg),
		},
	})
	if err != nil {
		zap.L().Warn("chat failed", zap.Error(err))
		return
	}
	ai.stats.MsgSent()
}
