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
	SessionID      uint32
	GateID         string
	GateInstanceID string
}

// GateCommandBuilder abstracts GateCommand + event payload construction.
// Each service provides its own implementation using its local proto imports.
type GateCommandBuilder interface {
	// BuildPushCommand builds a serialized GateCommand wrapping a PushToPlayerEvent.
	BuildPushCommand(sessionID uint32, gateInstanceID string,
		messageID uint32, body []byte) ([]byte, error)

	// BuildBroadcastCommand builds a serialized GateCommand wrapping a BroadcastToPlayersEvent.
	// When len(sessionList) >= BitmapThreshold and bitmap is smaller, bitmap encoding is used automatically.
	BuildBroadcastCommand(sessionList []uint32, gateInstanceID string,
		messageID uint32, body []byte) ([]byte, error)

	// BuildBroadcastToSceneCommand builds a serialized GateCommand wrapping a BroadcastToSceneEvent.
	BuildBroadcastToSceneCommand(sceneID uint64, gateInstanceID string,
		messageID uint32, body []byte) ([]byte, error)

	// BuildBroadcastToAllCommand builds a serialized GateCommand wrapping a BroadcastToAllEvent.
	BuildBroadcastToAllCommand(gateInstanceID string,
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
		sessions   []uint32
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

// GateInfo holds the minimum info needed to address a Gate node via Kafka.
type GateInfo struct {
	GateID         string
	GateInstanceID string
}

// BroadcastToScene sends a scene-wide broadcast to specific gates via Kafka.
// The caller provides the list of gates that have players in the scene.
func BroadcastToScene(ctx context.Context, w *kafkago.Writer, builder GateCommandBuilder,
	sceneID uint64, gates []GateInfo, messageID uint32, body []byte,
) error {
	var firstErr error
	for _, g := range gates {
		cmdBytes, err := builder.BuildBroadcastToSceneCommand(sceneID, g.GateInstanceID, messageID, body)
		if err != nil {
			logx.Errorf("BroadcastToScene: build command for gate-%s: %v", g.GateID, err)
			if firstErr == nil {
				firstErr = err
			}
			continue
		}

		topic := fmt.Sprintf("gate-%s", g.GateID)
		if err := w.WriteMessages(ctx, kafkago.Message{
			Topic: topic,
			Key:   []byte(g.GateID),
			Value: cmdBytes,
		}); err != nil {
			logx.Errorf("BroadcastToScene: send to %s failed: %v", topic, err)
			if firstErr == nil {
				firstErr = err
			}
		}
	}
	return firstErr
}

// BroadcastToAll sends a server-wide broadcast to all provided gates via Kafka.
func BroadcastToAll(ctx context.Context, w *kafkago.Writer, builder GateCommandBuilder,
	gates []GateInfo, messageID uint32, body []byte,
) error {
	var firstErr error
	for _, g := range gates {
		cmdBytes, err := builder.BuildBroadcastToAllCommand(g.GateInstanceID, messageID, body)
		if err != nil {
			logx.Errorf("BroadcastToAll: build command for gate-%s: %v", g.GateID, err)
			if firstErr == nil {
				firstErr = err
			}
			continue
		}

		topic := fmt.Sprintf("gate-%s", g.GateID)
		if err := w.WriteMessages(ctx, kafkago.Message{
			Topic: topic,
			Key:   []byte(g.GateID),
			Value: cmdBytes,
		}); err != nil {
			logx.Errorf("BroadcastToAll: send to %s failed: %v", topic, err)
			if firstErr == nil {
				firstErr = err
			}
		}
	}
	return firstErr
}

// BitmapThreshold is the minimum session count to consider bitmap encoding.
const BitmapThreshold = 32

// EncodeBitmapFields computes bitmap encoding for a session list.
// Returns (base, bitmap, useBitmap). When useBitmap is false, caller should use plain list.
func EncodeBitmapFields(sessions []uint32) (base uint32, bitmap []byte, useBitmap bool) {
	if len(sessions) < BitmapThreshold {
		return 0, nil, false
	}

	minID := sessions[0]
	maxID := sessions[0]
	for _, s := range sessions[1:] {
		if s < minID {
			minID = s
		}
		if s > maxID {
			maxID = s
		}
	}

	span := maxID - minID + 1
	bitmapBytes := (span + 7) / 8

	// Use bitmap when it is smaller than varint list (~3 bytes per uint32 varint)
	if int(bitmapBytes)+6 >= len(sessions)*3 {
		return 0, nil, false
	}

	bm := make([]byte, bitmapBytes)
	for _, s := range sessions {
		offset := s - minID
		bm[offset/8] |= 1 << (offset % 8)
	}
	return minID, bm, true
}
