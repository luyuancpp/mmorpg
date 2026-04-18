package config

import (
	"fmt"
	"os"

	"go.uber.org/zap"
	"gopkg.in/yaml.v3"

	"robot/logic/ai"
	"shared/generated/table"
)

// Config holds the robot load-test configuration.
type Config struct {
	GatewayAddr    string   `yaml:"gateway_addr"`    // e.g. "http://127.0.0.1:8081" (Gateway HTTP API, for /api/assign-gate)
	ZoneID         uint32   `yaml:"zone_id"`         // target zone (0 = auto-select from server-list)
	RobotCount     int      `yaml:"robot_count"`     // number of concurrent robots
	AccountFmt     string   `yaml:"account_fmt"`     // printf pattern, e.g. "robot_%04d"
	Password       string   `yaml:"password"`        // shared password for all robots
	SkillIDs       []uint32 `yaml:"skill_ids"`       // skill_table IDs from Skill.xlsx to cycle (empty = auto-read from class table)
	ActionInterval int      `yaml:"action_interval"` // seconds between AI actions
	ReportInterval int      `yaml:"report_interval"` // seconds between stats reports
	TableDir       string   `yaml:"table_dir"`       // path to generated/tables dir for reading Skill/Class tables

	// Mode: "stress" (default) — mass concurrent bots; "login-test" — scenario suite with login/skill/scene tests
	Mode string `yaml:"mode"`

	// AI behavior profile: "stress", "behavioral", "fighter", "explorer", "chatter"
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
		RobotCount:     1,
		AccountFmt:     "robot_%04d",
		Password:       "123456",
		ActionInterval: 3,
		ReportInterval: 5,
		TableDir:       "../generated/tables",
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
	if c.GatewayAddr == "" {
		return fmt.Errorf("gateway_addr must be set")
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

// LoadTables loads all config tables from TableDir and auto-resolves SkillIDs if empty.
func (c *Config) LoadTables() {
	if c.TableDir == "" {
		zap.L().Info("table_dir not set, skipping table loading")
		return
	}

	// Load all tables (same as other Go services).
	table.LoadTables(c.TableDir, false)

	zap.L().Info("loaded tables",
		zap.String("table_dir", c.TableDir),
		zap.Int("skill_count", table.SkillTableManagerInstance.Count()),
		zap.Int("class_count", table.ClassTableManagerInstance.Count()),
	)

	// If skill_ids not configured, read all skill IDs from the skill table.
	if len(c.SkillIDs) == 0 {
		for _, row := range table.SkillTableManagerInstance.FindAll() {
			c.SkillIDs = append(c.SkillIDs, row.Id)
		}
		zap.L().Info("auto-resolved skill_ids from skill table", zap.Uint32s("skill_ids", c.SkillIDs))
	} else {
		// Validate configured skill_ids exist in the table.
		var valid []uint32
		for _, id := range c.SkillIDs {
			if table.SkillTableManagerInstance.Exists(id) {
				valid = append(valid, id)
			} else {
				zap.L().Warn("configured skill_id not found in Skill table, skipping", zap.Uint32("skill_id", id))
			}
		}
		c.SkillIDs = valid
	}
}
