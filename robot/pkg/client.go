package pkg

import (
	"fmt"
	"sync/atomic"

	"github.com/golang/protobuf/proto"
	"github.com/luyuancpp/muduoclient/muduo"
	"go.uber.org/zap"

	"robot/proto/common/base"
)

// GameClient wraps a muduo TCP connection to a gate node.
// Each GameClient runs in its own goroutine (one-client-one-goroutine).
type GameClient struct {
	client   *muduo.Client
	PlayerId uint64
	Account  string
	seq      atomic.Uint64
}

// NewGameClient connects to the gate at ip:port.
func NewGameClient(ip string, port int) (*GameClient, error) {
	codec := &muduo.TcpCodec{}
	c, err := muduo.NewClient(ip, port, codec)
	if err != nil {
		return nil, fmt.Errorf("connect to gate %s:%d: %w", ip, port, err)
	}
	return &GameClient{client: c}, nil
}

// SendRequest sends a ClientRequest to the gate.
func (gc *GameClient) SendRequest(messageId uint32, body proto.Message) error {
	bodyBytes, err := proto.Marshal(body)
	if err != nil {
		return fmt.Errorf("marshal body: %w", err)
	}
	seq := gc.seq.Add(1)
	req := &base.ClientRequest{
		Id:        seq,
		MessageId: messageId,
		Body:      bodyBytes,
	}
	gc.client.Send(req)
	return nil
}

// RecvOne reads and returns a single MessageContent from the gate.
func (gc *GameClient) RecvOne() (*base.MessageContent, error) {
	msg, err := gc.client.Recv()
	if err != nil {
		return nil, err
	}
	switch m := msg.(type) {
	case *base.MessageContent:
		return m, nil
	default:
		return nil, fmt.Errorf("unexpected message type: %T", msg)
	}
}

// RecvLoop reads messages from the gate and dispatches them via onMessage.
func (gc *GameClient) RecvLoop(onMessage func(*GameClient, *base.MessageContent)) {
	for {
		msg, err := gc.client.Recv()
		if err != nil {
			zap.L().Error("recv error", zap.String("account", gc.Account), zap.Error(err))
			return
		}
		switch m := msg.(type) {
		case *base.MessageContent:
			onMessage(gc, m)
		default:
			zap.L().Warn("unexpected message type", zap.String("type", fmt.Sprintf("%T", msg)))
		}
	}
}

// Close shuts down the connection.
func (gc *GameClient) Close() {
	gc.client.Close()
}
