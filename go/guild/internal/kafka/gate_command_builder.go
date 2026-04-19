package kafka

import (
	"fmt"

	"google.golang.org/protobuf/proto"

	basepb "proto/common/base"
	kafkapb "proto/contracts/kafka"

	"guild/generated/pb/game"
	"shared/kafkautil"
)

// gateCommandBuilder implements kafkautil.GateCommandBuilder using local proto imports.
type gateCommandBuilder struct{}

// NewGateCommandBuilder returns a GateCommandBuilder for the guild service.
func NewGateCommandBuilder() kafkautil.GateCommandBuilder {
	return &gateCommandBuilder{}
}

func (b *gateCommandBuilder) BuildPushCommand(sessionID uint64, gateInstanceID string,
	messageID uint32, body []byte,
) ([]byte, error) {
	mc := &basepb.MessageContent{
		MessageId:         messageID,
		SerializedMessage: body,
	}
	event := &kafkapb.PushToPlayerEvent{
		SessionId:      sessionID,
		MessageContent: mc,
	}
	payload, err := proto.Marshal(event)
	if err != nil {
		return nil, fmt.Errorf("marshal PushToPlayerEvent: %w", err)
	}

	cmd := &kafkapb.GateCommand{
		EventId:          uint32(game.ContractsKafkaPushToPlayerEventEventId),
		TargetInstanceId: gateInstanceID,
		Payload:          payload,
	}
	return proto.Marshal(cmd)
}

func (b *gateCommandBuilder) BuildBroadcastCommand(sessionList []uint32, gateInstanceID string,
	messageID uint32, body []byte,
) ([]byte, error) {
	mc := &basepb.MessageContent{
		MessageId:         messageID,
		SerializedMessage: body,
	}
	event := &kafkapb.BroadcastToPlayersEvent{
		SessionList:    sessionList,
		MessageContent: mc,
	}
	payload, err := proto.Marshal(event)
	if err != nil {
		return nil, fmt.Errorf("marshal BroadcastToPlayersEvent: %w", err)
	}

	cmd := &kafkapb.GateCommand{
		EventId:          uint32(game.ContractsKafkaBroadcastToPlayersEventEventId),
		TargetInstanceId: gateInstanceID,
		Payload:          payload,
	}
	return proto.Marshal(cmd)
}
