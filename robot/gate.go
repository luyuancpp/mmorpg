package main

import (
	"context"
	"fmt"
	"net"
	"time"

	"go.uber.org/zap"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"

	"robot/config"
	"robot/proto/login"
)

type gateAssignment struct {
	addr      string
	payload   []byte
	signature []byte
}

// resolveGateAddr picks the best gate endpoint.
// If login_addr is configured, calls LoginPreGate.AssignGate to get the
// assigned gate + signed HMAC token with retry. Otherwise falls back to static gate_addr.
func resolveGateAddr(cfg *config.Config) (host, port string, payload, signature []byte, err error) {
	if cfg.LoginAddr != "" {
		const maxRetries = 30
		backoff := 2 * time.Second

		for attempt := 1; attempt <= maxRetries; attempt++ {
			result, assignErr := assignGate(cfg.LoginAddr, 0)
			if assignErr == nil {
				zap.L().Info("assigned gate", zap.String("gate", result.addr), zap.Int("attempt", attempt))
				h, p, splitErr := net.SplitHostPort(result.addr)
				return h, p, result.payload, result.signature, splitErr
			}

			zap.L().Warn("AssignGate failed, retrying",
				zap.String("login_addr", cfg.LoginAddr),
				zap.Int("attempt", attempt),
				zap.Int("max", maxRetries),
				zap.Error(assignErr),
			)

			if attempt < maxRetries {
				time.Sleep(backoff)
				if backoff < 10*time.Second {
					backoff = backoff * 3 / 2 // 2s → 3s → 4.5s → 6.75s → 10s cap
				}
			}
		}

		// All retries exhausted; fall back to static gate_addr if available.
		if cfg.GateAddr == "" {
			return "", "", nil, nil, fmt.Errorf("AssignGate failed after %d retries and no static gate_addr configured", maxRetries)
		}
		zap.L().Warn("AssignGate retries exhausted, falling back to gate_addr",
			zap.String("gate_addr", cfg.GateAddr),
		)
	}
	h, p, splitErr := net.SplitHostPort(cfg.GateAddr)
	return h, p, nil, nil, splitErr
}

// assignGate calls LoginPreGate.AssignGate gRPC and returns the gate address
// plus the HMAC connection token.
func assignGate(loginAddr string, zoneId uint32) (*gateAssignment, error) {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	conn, err := grpc.NewClient(loginAddr, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		return nil, fmt.Errorf("grpc dial %s: %w", loginAddr, err)
	}
	defer conn.Close()

	resp, err := login.NewLoginPreGateClient(conn).AssignGate(ctx, &login.AssignGateRequest{ZoneId: zoneId})
	if err != nil {
		return nil, fmt.Errorf("AssignGate: %w", err)
	}
	if resp.Error != "" {
		return nil, fmt.Errorf("AssignGate: %s", resp.Error)
	}
	if resp.Ip == "" || resp.Port == 0 {
		return nil, fmt.Errorf("AssignGate returned empty address")
	}

	return &gateAssignment{
		addr:      fmt.Sprintf("%s:%d", resp.Ip, resp.Port),
		payload:   resp.TokenPayload,
		signature: resp.TokenSignature,
	}, nil
}
