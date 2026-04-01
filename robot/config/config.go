package config

import (
	"fmt"
	"os"

	"gopkg.in/yaml.v3"

	"robot/logic/ai"
)

// Config holds the robot load-test configuration.
type Config struct {
	GateAddr       string   `yaml:"gate_addr"`       // e.g. "127.0.0.1:6000" (fallback if login_addr is empty)
	LoginAddr      string   `yaml:"login_addr"`      // e.g. "127.0.0.1:50000" (gRPC, for GetGateList)
	RobotCount     int      `yaml:"robot_count"`     // number of concurrent robots
	AccountFmt     string   `yaml:"account_fmt"`     // printf pattern, e.g. "robot_%04d"
	Password       string   `yaml:"password"`        // shared password for all robots
	SkillIDs       []uint32 `yaml:"skill_ids"`       // skill table IDs to cycle
	ActionInterval int      `yaml:"action_interval"` // seconds between AI actions
	ReportInterval int      `yaml:"report_interval"` // seconds between stats reports

	// Mode: "stress" (default) — mass concurrent bots; "login-test" — login scenario test suite
	Mode string `yaml:"mode"`

	// AI behavior profile: "stress" (default, pure skill spam), "fighter", "explorer", "chatter"
	// Or set custom_weights to define your own action distribution.
	Profile       string         `yaml:"profile"`
	CustomWeights map[string]int `yaml:"custom_weights"` // e.g. {cast_skill: 50, move: 30, idle: 15, chat: 5}

	// LLM settings (optional). When enabled, LLM decides actions instead of profile weights.
	// Best for 1-10 robots to model realistic player behavior. Not for mass load testing.
	LLM LLMConfig `yaml:"llm"`
}

// LLMConfig configures the OpenAI-compatible endpoint for AI-driven behavior.
type LLMConfig struct {
	Enabled  bool   `yaml:"enabled"`
	Endpoint string `yaml:"endpoint"` // e.g. "http://localhost:11434/v1/chat/completions"
	APIKey   string `yaml:"api_key"`  // optional, for OpenAI/Azure
	Model    string `yaml:"model"`    // e.g. "gpt-4o-mini", "llama3", "qwen2"
}

func Load(path string) (*Config, error) {
	data, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	cfg := &Config{
		GateAddr:       "127.0.0.1:6000",
		RobotCount:     1,
		AccountFmt:     "robot_%04d",
		Password:       "123456",
		SkillIDs:       []uint32{1001},
		ActionInterval: 3,
		ReportInterval: 5,
	}
	if err := yaml.Unmarshal(data, cfg); err != nil {
		return nil, err
	}
	if err := cfg.validate(); err != nil {
		return nil, fmt.Errorf("config validation: %w", err)
	}
	return cfg, nil
}

// ResolveProfile returns the AI profile to use.
// Priority: custom_weights > builtin profile name > nil (use default).
func (c *Config) ResolveProfile() *ai.Profile {
	if len(c.CustomWeights) > 0 {
		weights := make(map[ai.Action]int)
		for name, w := range c.CustomWeights {
			if a, ok := ai.ParseAction(name); ok {
				weights[a] = w
			}
		}
		p := &ai.Profile{Name: "custom", Weights: weights}
		return p
	}
	if bp, ok := ai.BuiltinProfiles[c.Profile]; ok {
		return &bp
	}
	return nil
}

func (c *Config) validate() error {
	if c.GateAddr == "" && c.LoginAddr == "" {
		return fmt.Errorf("gate_addr or login_addr must be set")
	}
	if c.RobotCount <= 0 {
		return fmt.Errorf("robot_count must be > 0")
	}
	if c.AccountFmt == "" {
		return fmt.Errorf("account_fmt must be set")
	}
	switch c.Mode {
	case "", "stress", "login-test":
		// valid
	default:
		return fmt.Errorf("unknown mode %q (expected stress or login-test)", c.Mode)
	}
	return nil
}
