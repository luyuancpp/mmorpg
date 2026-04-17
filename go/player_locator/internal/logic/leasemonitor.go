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

	"player_locator/internal/svc"
	kafkapb "proto/contracts/kafka"
	pb "proto/player_locator"
	smpb "proto/scene_manager"
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
	// AFK pass check: if the player has an active AFK pass, extend the lease instead of expiring.
	if hasActiveAfkPass(ctx, svcCtx, playerID) {
		extendLeaseForAfkPass(ctx, svcCtx, playerID)
		logx.Infof("LeaseMonitor: player %d has active AFK pass, lease extended", playerID)
		return
	}

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

	// Clean up both session and location keys in one round-trip
	keys := []string{key, locationKey(int64(playerID))}
	if err := svcCtx.RedisClient.Del(ctx, keys...).Err(); err != nil {
		logx.Errorf("LeaseMonitor: failed to delete keys for player %d: %v", playerID, err)
	}

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

	// Notify SceneManager to clean up player location + decrement instance player count.
	notifySceneManagerLeave(ctx, svcCtx, session)

	logx.Infof("LeaseMonitor: lease expired player=%d session=%d gate=%s scene=%s",
		playerID, session.SessionId, session.GateId, session.SceneNodeId)
}

// ContractsKafkaPlayerLeaseExpiredEventEventId matches event_id.txt entry 34.
// TODO: use generated constant once event_id.go is produced for player_locator.
const contractsKafkaPlayerLeaseExpiredEventEventId uint32 = 34

func sendLeaseExpiredToGate(ctx context.Context, svcCtx *svc.ServiceContext, session *pb.PlayerSession) {
	if session.GateId == "" {
		return
	}

	// Build inner event payload (use contracts/kafka type to match C++ dispatch)
	event := &kafkapb.PlayerLeaseExpiredEvent{
		PlayerId:       session.PlayerId,
		SessionId:      session.SessionId,
		SceneNodeId:    session.SceneNodeId,
		SceneId:        session.SceneId,
		GateId:         session.GateId,
		GateInstanceId: session.GateInstanceId,
	}
	eventPayload, err := proto.Marshal(event)
	if err != nil {
		logx.Errorf("LeaseMonitor: marshal PlayerLeaseExpiredEvent failed: %v", err)
		return
	}

	// Wrap in GateCommand for the Gate Kafka consumer
	eventId := contractsKafkaPlayerLeaseExpiredEventEventId
	cmd := &kafkapb.GateCommand{
		PlayerId:         session.PlayerId,
		SessionId:        session.SessionId,
		Payload:          eventPayload,
		TargetGateId:     parseGateID(session.GateId),
		TargetInstanceId: session.GateInstanceId,
		EventId:          eventId,
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
	id, err := strconv.ParseUint(gateID, 10, 32)
	if err != nil {
		logx.Errorf("parseGateID: invalid gateID %q: %v", gateID, err)
		return 0
	}
	return uint32(id)
}

// notifySceneManagerLeave calls SceneManager.LeaveScene to clean up player location
// and decrement the instance player count. Best-effort: failures are logged, not fatal.
func notifySceneManagerLeave(ctx context.Context, svcCtx *svc.ServiceContext, session *pb.PlayerSession) {
	if svcCtx.SceneManagerClient == nil {
		return
	}
	if session.SceneId == 0 {
		return
	}

	_, err := svcCtx.SceneManagerClient.LeaveScene(ctx, &smpb.LeaveSceneRequest{
		PlayerId: session.PlayerId,
		SceneId:  session.SceneId,
	})
	if err != nil {
		logx.Errorf("LeaseMonitor: SceneManager.LeaveScene failed for player %d scene %d: %v",
			session.PlayerId, session.SceneId, err)
		return
	}
	logx.Infof("LeaseMonitor: notified SceneManager.LeaveScene player=%d scene=%d",
		session.PlayerId, session.SceneId)
}

// --- AFK Pass (挂机月卡) ---

const (
	afkPassKeyPrefix  = "player:afk_pass:"
	afkPassLeaseTTLSeconds = 300 // re-lease for 5 minutes when AFK pass is active
)

func afkPassKey(playerID uint64) string {
	return fmt.Sprintf("%s%d", afkPassKeyPrefix, playerID)
}

// hasActiveAfkPass checks if the player has a valid AFK pass in Redis.
// Key: player:afk_pass:{player_id} -> expiry unix timestamp (set by purchase/activation flow).
func hasActiveAfkPass(ctx context.Context, svcCtx *svc.ServiceContext, playerID uint64) bool {
	val, err := svcCtx.RedisClient.Get(ctx, afkPassKey(playerID)).Result()
	if err != nil {
		return false
	}
	expiry, err := strconv.ParseInt(val, 10, 64)
	if err != nil {
		return false
	}
	return time.Now().Unix() < expiry
}

// extendLeaseForAfkPass re-adds the player to the lease ZSET so they stay
// "disconnecting" but don't get cleaned up until the AFK pass expires.
func extendLeaseForAfkPass(ctx context.Context, svcCtx *svc.ServiceContext, playerID uint64) {
	newDeadline := time.Now().Add(time.Duration(afkPassLeaseTTLSeconds) * time.Second).Unix()

	if err := svcCtx.RedisClient.ZAdd(ctx, LeaseZSetKey, redis.Z{
		Score:  float64(newDeadline),
		Member: fmt.Sprintf("%d", playerID),
	}).Err(); err != nil {
		logx.Errorf("LeaseMonitor: failed to extend lease for AFK player %d: %v", playerID, err)
	}
}
