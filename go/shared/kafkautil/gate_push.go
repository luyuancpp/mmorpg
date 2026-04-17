package kafkautil

import (
	"context"
	"fmt"
	"strconv"

	kafkago "github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
)

// PlayerGateInfo holds the minimum session info needed to route a push to the right Gate.
type PlayerGateInfo struct {
	PlayerID       uint64
	SessionID      uint64
	GateID         string
	GateInstanceID string
}

// GateCommandBuilder abstracts GateCommand + event payload construction.
// Each service provides its own implementation using its local proto imports.
type GateCommandBuilder interface {
	// BuildPushCommand builds a serialized GateCommand wrapping a PushToPlayerEvent.
	BuildPushCommand(sessionID uint64, gateInstanceID string,
		messageID uint32, body []byte) ([]byte, error)

	// BuildBroadcastCommand builds a serialized GateCommand wrapping a BroadcastToPlayersEvent.
	BuildBroadcastCommand(sessionList []uint64, gateInstanceID string,
		messageID uint32, body []byte) ([]byte, error)
}

// PushToPlayer sends a single message to a player's client via Kafka -> Gate -> TCP.
func PushToPlayer(ctx context.Context, w *kafkago.Writer, builder GateCommandBuilder,
	info PlayerGateInfo, messageID uint32, body []byte,
) error {
	cmdBytes, err := builder.BuildPushCommand(info.SessionID, info.GateInstanceID, messageID, body)
	if err != nil {
		return fmt.Errorf("build push command: %w", err)
	}

	topic := fmt.Sprintf("gate-%s", info.GateID)
	return w.WriteMessages(ctx, kafkago.Message{
		Topic: topic,
		Key:   []byte(strconv.FormatUint(info.PlayerID, 10)),
		Value: cmdBytes,
	})
}

// BroadcastToPlayers groups players by gate_id and sends one Kafka message per Gate.
// The message body is serialized once per gate group.
func BroadcastToPlayers(ctx context.Context, w *kafkago.Writer, builder GateCommandBuilder,
	players []PlayerGateInfo, messageID uint32, body []byte,
) error {
	type gateGroup struct {
		instanceID string
		sessions   []uint64
	}
	grouped := make(map[string]*gateGroup)
	for _, p := range players {
		g, ok := grouped[p.GateID]
		if !ok {
			g = &gateGroup{instanceID: p.GateInstanceID}
			grouped[p.GateID] = g
		}
		g.sessions = append(g.sessions, p.SessionID)
	}

	var firstErr error
	for gateID, g := range grouped {
		cmdBytes, err := builder.BuildBroadcastCommand(g.sessions, g.instanceID, messageID, body)
		if err != nil {
			logx.Errorf("BroadcastToPlayers: build command for gate-%s: %v", gateID, err)
			if firstErr == nil {
				firstErr = err
			}
			continue
		}

		topic := fmt.Sprintf("gate-%s", gateID)
		if err := w.WriteMessages(ctx, kafkago.Message{
			Topic: topic,
			Key:   []byte(gateID),
			Value: cmdBytes,
		}); err != nil {
			logx.Errorf("BroadcastToPlayers: send to %s failed: %v", topic, err)
			if firstErr == nil {
				firstErr = err
			}
		}
	}

	return firstErr
}
