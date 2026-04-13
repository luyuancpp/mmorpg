package ai

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"strings"
	"time"

	"go.uber.org/zap"
)

// LLMAdvisor calls an OpenAI-compatible chat completion endpoint to decide
// the robot's next action based on current game state.
//
// Works with: OpenAI, Azure OpenAI, Ollama, vLLM, or any compatible proxy.
//
// For load testing (thousands of robots): don't enable this — use profiles.
// For behavior modeling (1-10 robots): enable to generate realistic action traces.
type LLMAdvisor struct {
	endpoint string // e.g. "http://localhost:11434/v1/chat/completions"
	apiKey   string
	model    string
	client   *http.Client
}

func NewLLMAdvisor(endpoint, apiKey, model string) *LLMAdvisor {
	return &LLMAdvisor{
		endpoint: endpoint,
		apiKey:   apiKey,
		model:    model,
		client:   &http.Client{Timeout: 5 * time.Second},
	}
}

// GameState is a snapshot of the robot's world for the LLM to reason about.
type GameState struct {
	PlayerId  uint64  `json:"player_id"`
	PosX      float64 `json:"pos_x"`
	PosZ      float64 `json:"pos_z"`
	SkillIDs  []uint32 `json:"skill_ids"`
	TickCount int      `json:"tick_count"`
}

// DecideAction asks the LLM which action to take next.
// Returns the action and an optional free-text reason (for logging/training).
func (a *LLMAdvisor) DecideAction(state GameState) (Action, string) {
	stateJSON, _ := json.Marshal(state)

	prompt := fmt.Sprintf(
		`You are controlling a game character. Based on the current state, pick ONE action.
Available actions: idle, move, cast_skill, chat
Current state: %s
Reply with ONLY the action name (one word).`, stateJSON)

	action, reason, err := a.chatCompletion(prompt)
	if err != nil {
		zap.L().Warn("llm decide failed, fallback to idle", zap.Error(err))
		return ActionIdle, ""
	}
	return action, reason
}

func (a *LLMAdvisor) chatCompletion(userPrompt string) (Action, string, error) {
	reqBody := map[string]any{
		"model": a.model,
		"messages": []map[string]string{
			{"role": "system", "content": "You control a game robot. Reply with only an action name: idle, move, cast_skill, chat."},
			{"role": "user", "content": userPrompt},
		},
		"max_tokens":  32,
		"temperature": 0.7,
	}

	bodyBytes, err := json.Marshal(reqBody)
	if err != nil {
		return ActionIdle, "", err
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	req, err := http.NewRequestWithContext(ctx, http.MethodPost, a.endpoint, bytes.NewReader(bodyBytes))
	if err != nil {
		return ActionIdle, "", err
	}
	req.Header.Set("Content-Type", "application/json")
	if a.apiKey != "" {
		req.Header.Set("Authorization", "Bearer "+a.apiKey)
	}

	resp, err := a.client.Do(req)
	if err != nil {
		return ActionIdle, "", err
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		b, _ := io.ReadAll(resp.Body)
		return ActionIdle, "", fmt.Errorf("llm api %d: %s", resp.StatusCode, string(b))
	}

	var result struct {
		Choices []struct {
			Message struct {
				Content string `json:"content"`
			} `json:"message"`
		} `json:"choices"`
	}
	if err := json.NewDecoder(resp.Body).Decode(&result); err != nil {
		return ActionIdle, "", err
	}
	if len(result.Choices) == 0 {
		return ActionIdle, "", fmt.Errorf("no choices in llm response")
	}

	raw := strings.TrimSpace(result.Choices[0].Message.Content)
	// LLM might return "cast_skill" or "I think cast_skill" — extract the action word.
	for _, word := range strings.Fields(strings.ToLower(raw)) {
		if action, ok := ParseAction(word); ok {
			return action, raw, nil
		}
	}
	return ActionIdle, raw, nil
}
