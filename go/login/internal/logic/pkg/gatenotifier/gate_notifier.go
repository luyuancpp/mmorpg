// Package gatenotifier sends commands to Gate via Kafka, replacing Centre's direct RPC.
package gatenotifier

import (
	"context"
	"fmt"

	"github.com/IBM/sarama"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
)

// GateNotifier sends GateCommand messages to gate-{gate_id} Kafka topics.
type GateNotifier struct {
	producer sarama.SyncProducer
}

// NewGateNotifier creates a notifier using an existing Sarama SyncProducer.
func NewGateNotifier(producer sarama.SyncProducer) *GateNotifier {
	return &GateNotifier{producer: producer}
}

// SendGateCommand sends a protobuf message to the gate's Kafka topic.
func (n *GateNotifier) SendGateCommand(ctx context.Context, gateID string, msg proto.Message) error {
	data, err := proto.Marshal(msg)
	if err != nil {
		return fmt.Errorf("marshal gate command: %w", err)
	}

	topic := fmt.Sprintf("gate-%s", gateID)
	_, _, err = n.producer.SendMessage(&sarama.ProducerMessage{
		Topic: topic,
		Value: sarama.ByteEncoder(data),
	})
	if err != nil {
		logx.Errorf("Failed to send gate command to topic %s: %v", topic, err)
		return err
	}
	logx.Infof("Sent gate command to topic %s", topic)
	return nil
}
