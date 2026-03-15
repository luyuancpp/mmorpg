package logic

import (
	"context"
	"fmt"
	"strconv"
	"time"

	"github.com/redis/go-redis/v9"
	kafkago "github.com/segmentio/kafka-go"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"

	kafkapb "player_locator/contracts/kafka"
	"player_locator/internal/svc"
	pb "player_locator/proto/player_locator"
)

// popExpiredLeasesScript atomically fetches and removes expired leases.
// Safe for concurrent execution across multiple player_locator instances.
var popExpiredLeasesScript = redis.NewScript(`
local expired = redis.call("ZRANGEBYSCORE", KEYS[1], "-inf", ARGV[1], "LIMIT", 0, ARGV[2])
if #expired > 0 then
    redis.call("ZREM", KEYS[1], unpack(expired))
end
return expired
`)

// StartLeaseMonitor polls for expired disconnect leases and publishes cleanup events.
func StartLeaseMonitor(ctx context.Context, svcCtx *svc.ServiceContext, pollInterval time.Duration, batchSize int) {
	if pollInterval <= 0 {
		pollInterval = time.Second
	}
	if batchSize <= 0 {
		batchSize = 100
	}

	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()

	logx.Infof("LeaseMonitor started: poll=%v batch=%d", pollInterval, batchSize)

	for {
		select {
		case <-ctx.Done():
			logx.Info("LeaseMonitor stopped")
			return
		case <-ticker.C:
			processExpiredLeases(ctx, svcCtx, batchSize)
		}
	}
}

func processExpiredLeases(ctx context.Context, svcCtx *svc.ServiceContext, batchSize int) {
	now := time.Now().Unix()

	result, err := popExpiredLeasesScript.Run(ctx, svcCtx.RedisClient,
		[]string{LeaseZSetKey},
		now, batchSize,
	).StringSlice()

	if err != nil && err != redis.Nil {
		logx.Errorf("LeaseMonitor: pop expired leases failed: %v", err)
		return
	}

	for _, playerIDStr := range result {
		playerID, err := strconv.ParseUint(playerIDStr, 10, 64)
		if err != nil {
			logx.Errorf("LeaseMonitor: invalid player_id in lease set: %s", playerIDStr)
			continue
		}

		handleLeaseExpiry(ctx, svcCtx, playerID)
	}
}

func handleLeaseExpiry(ctx context.Context, svcCtx *svc.ServiceContext, playerID uint64) {
	key := sessionKey(playerID)

	// Session may already be gone (Redis TTL expired), read what we can
	data, err := svcCtx.RedisClient.Get(ctx, key).Bytes()

	var session *pb.PlayerSession
	if err == nil {
		session = &pb.PlayerSession{}
		if unmarshalErr := proto.Unmarshal(data, session); unmarshalErr != nil {
			session = nil
		}
	}

	// Clean up both session and location keys
	svcCtx.RedisClient.Del(ctx, key)
	svcCtx.RedisClient.Del(ctx, locationKey(int64(playerID)))

	if session == nil {
		logx.Infof("LeaseMonitor: session already gone for player %d, keys cleaned", playerID)
		return
	}

	// Only act on sessions that are still in DISCONNECTING state
	if session.State != pb.PlayerSessionState_SESSION_STATE_DISCONNECTING {
		logx.Infof("LeaseMonitor: player %d state=%v (not disconnecting), skipping notification",
			playerID, session.State)
		return
	}

	sendLeaseExpiredToGate(ctx, svcCtx, session)

	logx.Infof("LeaseMonitor: lease expired player=%d session=%d gate=%s scene=%s",
		playerID, session.SessionId, session.GateId, session.SceneNodeId)
}

func sendLeaseExpiredToGate(ctx context.Context, svcCtx *svc.ServiceContext, session *pb.PlayerSession) {
	if session.GateId == "" {
		return
	}

	// Build inner event payload
	event := &pb.LeaseExpiredEvent{
		PlayerId:       session.PlayerId,
		SessionId:      session.SessionId,
		SceneNodeId:    session.SceneNodeId,
		SceneId:        session.SceneId,
		GateId:         session.GateId,
		GateInstanceId: session.GateInstanceId,
	}
	eventPayload, err := proto.Marshal(event)
	if err != nil {
		logx.Errorf("LeaseMonitor: marshal LeaseExpiredEvent failed: %v", err)
		return
	}

	// Wrap in GateCommand for the Gate Kafka consumer
	cmd := &kafkapb.GateCommand{
		CommandType:      kafkapb.GateCommand_LeaseExpired,
		PlayerId:         session.PlayerId,
		SessionId:        session.SessionId,
		Payload:          eventPayload,
		TargetGateId:     parseGateID(session.GateId),
		TargetInstanceId: session.GateInstanceId,
	}
	cmdBytes, err := proto.Marshal(cmd)
	if err != nil {
		logx.Errorf("LeaseMonitor: marshal GateCommand failed: %v", err)
		return
	}

	topic := fmt.Sprintf("gate-%s", session.GateId)
	err = svcCtx.KafkaWriter.WriteMessages(ctx, kafkago.Message{
		Topic: topic,
		Key:   []byte(fmt.Sprintf("%d", session.PlayerId)),
		Value: cmdBytes,
	})
	if err != nil {
		logx.Errorf("LeaseMonitor: publish LeaseExpired to %s failed: %v", topic, err)
	}
}

func parseGateID(gateID string) uint32 {
	id, _ := strconv.ParseUint(gateID, 10, 32)
	return uint32(id)
}
