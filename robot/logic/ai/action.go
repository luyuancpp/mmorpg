package ai

import "math/rand"

// Action represents a type of behavior a robot can perform.
type Action int

const (
	ActionIdle       Action = iota // do nothing this tick
	ActionMove                     // walk to a random nearby position
	ActionCastSkill                // release a skill at a random target/position
	ActionChat                     // send a chat message
)

var actionNames = map[Action]string{
	ActionIdle:      "idle",
	ActionMove:      "move",
	ActionCastSkill: "cast_skill",
	ActionChat:      "chat",
}

func (a Action) String() string {
	if s, ok := actionNames[a]; ok {
		return s
	}
	return "unknown"
}

// ParseAction converts a string (from LLM output or config) to an Action.
func ParseAction(s string) (Action, bool) {
	for a, name := range actionNames {
		if name == s {
			return a, true
		}
	}
	return ActionIdle, false
}

// Profile defines behavior weights for each action type.
// Higher weight = robot picks that action more often.
type Profile struct {
	Name    string
	Weights map[Action]int
}

// Pick selects a random action according to the weight distribution.
func (p *Profile) Pick() Action {
	total := 0
	for _, w := range p.Weights {
		total += w
	}
	if total == 0 {
		return ActionIdle
	}

	r := rand.Intn(total)
	for action, w := range p.Weights {
		r -= w
		if r < 0 {
			return action
		}
	}
	return ActionIdle
}

// Built-in profiles. Users can also define weights in robot.yaml.
var BuiltinProfiles = map[string]Profile{
	"fighter": {
		Name: "fighter",
		Weights: map[Action]int{
			ActionCastSkill: 60,
			ActionMove:      25,
			ActionIdle:      10,
			ActionChat:      5,
		},
	},
	"explorer": {
		Name: "explorer",
		Weights: map[Action]int{
			ActionMove:      50,
			ActionIdle:      20,
			ActionCastSkill: 20,
			ActionChat:      10,
		},
	},
	"chatter": {
		Name: "chatter",
		Weights: map[Action]int{
			ActionChat:      40,
			ActionIdle:      30,
			ActionMove:      20,
			ActionCastSkill: 10,
		},
	},
	// stress: pure skill spam, same as old behavior
	"stress": {
		Name: "stress",
		Weights: map[Action]int{
			ActionCastSkill: 100,
		},
	},
}
