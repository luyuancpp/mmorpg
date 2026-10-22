package main

// 单次真实TCP连接复用正式muduo TcpCodec；不使用自动重连，才能取得旧连接关闭的正证。
import (
	"context"
	"encoding/binary"
	"errors"
	"fmt"
	"io"
	"net"
	"syscall"
	"time"

	legacyproto "github.com/golang/protobuf/proto"
	"github.com/luyuancpp/muduoclient/muduo"
	"google.golang.org/protobuf/proto"
	"google.golang.org/protobuf/protoadapt"

	"proto/battle"
	base "proto/common/base"
	"robot/generated/pb/game"
)

type ownedWireDirect struct {
	conn net.Conn
	codec muduo.TcpCodec
	sequence uint64
}

type ownedWireTicketRejected struct{}
func (*ownedWireTicketRejected) Error() string { return "actual Battle token handshake explicitly rejected the original ticket" }

func dialOwnedWireDirect(ctx context.Context, assigned *battle.BattleAssignedS2C) (*ownedWireDirect, error) {
	deadline := time.Now().Add(ownedObserverWireTimeout)
	if parent, ok := ctx.Deadline(); ok && parent.Before(deadline) { deadline = parent }
	conn, err := (&net.Dialer{Timeout: ownedObserverWireTimeout}).DialContext(ctx, "tcp", net.JoinHostPort(assigned.GetHost(), fmt.Sprint(assigned.GetPort())))
	if err != nil { return nil, err }
	c := &ownedWireDirect{conn: conn}
	fail := func(err error) (*ownedWireDirect, error) { _ = conn.Close(); return nil, err }
	frame, err := c.frame(&battle.BattleTokenVerifyRequest{Payload: assigned.GetTokenPayload(), Signature: assigned.GetTokenSignature()})
	if err != nil { return fail(err) }
	if err := c.writeUntil(frame, deadline); err != nil { return fail(err) }
	message, err := c.readUntil(deadline)
	if err != nil { return fail(err) }
	verified, ok := message.(*battle.BattleTokenVerifyResponse)
	if !ok { return fail(fmt.Errorf("unexpected actual Battle handshake type %T", message)) }
	if !verified.GetSuccess() { return fail(&ownedWireTicketRejected{}) }
	if verified.GetBattleId() != assigned.GetBattleId() { return fail(errors.New("actual handshake changed battle ID")) }
	// D69必须是握手自动下发，不能先Get再把其响应当作首帧。
	message, err = c.readUntil(deadline)
	if err != nil { return fail(err) }
	content, ok := message.(*base.MessageContent)
	var snapshot battle.SpectateStateS2C
	if !ok || content.GetMessageId() != game.BattleClientPlayerNotifySpectateStateMessageId || content.GetErrorMessage().GetId() != 0 ||
		proto.Unmarshal(content.GetSerializedMessage(), &snapshot) != nil || snapshot.GetState().GetBattleId() != assigned.GetBattleId() {
		return fail(errors.New("actual observer handshake lacks original automatic SpectateState"))
	}
	return c, nil
}

func (c *ownedWireDirect) Close() { _ = c.conn.Close() }

func (c *ownedWireDirect) frame(message proto.Message) ([]byte, error) {
	var old legacyproto.Message = protoadapt.MessageV1Of(message)
	return c.codec.Encode(&old)
}

func (c *ownedWireDirect) write(message proto.Message) error {
	frame, err := c.frame(message)
	if err != nil { return err }
	return c.writeBytes(frame)
}

func (c *ownedWireDirect) writeBytes(frame []byte) error {
	return c.writeUntil(frame, time.Now().Add(ownedObserverWireTimeout))
}

func (c *ownedWireDirect) writeUntil(frame []byte, deadline time.Time) error {
	if err := c.conn.SetWriteDeadline(deadline); err != nil { return err }
	for len(frame) > 0 {
		n, err := c.conn.Write(frame)
		if err != nil { return err }
		if n == 0 { return io.ErrShortWrite }
		frame = frame[n:]
	}
	return nil
}

func (c *ownedWireDirect) read() (legacyproto.Message, error) {
	return c.readUntil(time.Now().Add(ownedObserverWireTimeout))
}

func (c *ownedWireDirect) readUntil(deadline time.Time) (legacyproto.Message, error) {
	if err := c.conn.SetReadDeadline(deadline); err != nil { return nil, err }
	header := make([]byte, 4)
	if _, err := io.ReadFull(c.conn, header); err != nil { return nil, err }
	size := binary.BigEndian.Uint32(header)
	if size < 8 || size > 2*1024*1024 { return nil, errors.New("actual muduo frame exceeds the bounded test reader") }
	frame := make([]byte, 4+int(size)); copy(frame, header)
	if _, err := io.ReadFull(c.conn, frame[4:]); err != nil { return nil, err }
	message, consumed, err := c.codec.Decode(frame)
	if err != nil || consumed != uint32(len(frame)) || message == nil { return nil, fmt.Errorf("actual official muduo decode incomplete: consumed=%d err=%v", consumed, err) }
	return message, nil
}

func (c *ownedWireDirect) request(id uint32, message proto.Message) (*base.ClientRequest, error) {
	body, err := proto.Marshal(message)
	if err != nil { return nil, err }
	c.sequence++
	return &base.ClientRequest{Id: c.sequence, MessageId: id, Body: body}, nil
}

func (c *ownedWireDirect) call(id uint32, message proto.Message) (*base.MessageContent, error) {
	request, err := c.request(id, message)
	if err != nil { return nil, err }
	if err := c.write(request); err != nil { return nil, err }
	deadline := time.Now().Add(ownedObserverWireTimeout)
	for time.Now().Before(deadline) {
		incoming, err := c.readUntil(deadline)
		if err != nil { return nil, err }
		content, ok := incoming.(*base.MessageContent)
		if !ok { return nil, fmt.Errorf("unexpected actual direct response type %T", incoming) }
		if content.GetId() != request.GetId() { continue }
		if content.GetMessageId() != id || content.GetErrorMessage().GetId() != 0 { return nil, errors.New("actual direct call received a bound rejection") }
		return content, nil
	}
	return nil, errors.New("actual direct call lacks a bound response within its monotonic budget")
}

func remoteWireClosed(err error) bool {
	// WSAECONNRESET=10054是Windows系统错误；syscall.ECONNRESET覆盖Unix。
	const windowsConnectionReset syscall.Errno = 10054
	return errors.Is(err, io.EOF) || errors.Is(err, syscall.ECONNRESET) || errors.Is(err, windowsConnectionReset)
}

// 两个旧请求一起写入原TCP：必须取得原连接EOF/reset或两个原调用的明确拒绝。
// 超时/任意transport error/异步状态增长均不计通过，避免自动重连客户端把未知当拒绝。
func (c *ownedWireDirect) staleClosed(battleID uint64) error {
	deadline := time.Now().Add(ownedObserverWireTimeout)
	get, err := c.request(game.BattleClientPlayerGetBattleStateMessageId, &battle.GetBattleStateRequest{BattleId: battleID})
	if err != nil { return err }
	stop, err := c.request(game.BattleClientPlayerStopWatchBattleMessageId, &battle.StopWatchBattleRequest{BattleId: battleID})
	if err != nil { return err }
	first, err := c.frame(get); if err != nil { return err }
	second, err := c.frame(stop); if err != nil { return err }
	if err := c.writeBytes(append(first, second...)); err != nil {
		if remoteWireClosed(err) { return nil }
		return err
	}
	denied := map[uint64]bool{}
	for len(denied) < 2 {
		incoming, err := c.readUntil(deadline)
		if remoteWireClosed(err) { return nil }
		if err != nil { return fmt.Errorf("stale connection closure remains unknown: %w", err) }
		content, ok := incoming.(*base.MessageContent)
		if !ok { return fmt.Errorf("stale connection returned unexpected %T", incoming) }
		if content.GetMessageId() == game.BattleClientPlayerNotifySpectateStateMessageId || content.GetMessageId() == game.BattleClientPlayerNotifySpectateTurnResultMessageId {
			return errors.New("stale connection still received observer state after replacement")
		}
		if content.GetId() == get.GetId() || content.GetId() == stop.GetId() {
			expected := get.GetMessageId()
			if content.GetId() == stop.GetId() { expected = stop.GetMessageId() }
			if content.GetMessageId() != expected { return errors.New("stale explicit rejection changed the original method") }
			if content.GetErrorMessage().GetId() == 0 { return errors.New("stale Get/Stop received a successful bound response") }
			denied[content.GetId()] = true
		}
	}
	return nil
}
