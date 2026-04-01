package config

import (
	"os"

	"gopkg.in/yaml.v3"
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

	// AI behavior profile: "stress" (default, pure skill spam), "fighter", "explorer", "chatter"
	Profile string `yaml:"profile"`

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
	return cfg, nil
}
