package logic

import (
	"context"
	"fmt"
	"strconv"
	"time"

	"data_service/internal/constants"
	"data_service/internal/svc"

	goredis "github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

const (
	playerDataPrefix   = "player:{%d}:"   // hash-tag ensures same slot
	playerVersionKey   = "player:{%d}:__version"
)

func playerField(playerID uint64, field string) string {
	return fmt.Sprintf("player:{%d}:%s", playerID, field)
}

func versionKey(playerID uint64) string {
	return fmt.Sprintf(playerVersionKey, playerID)
}

// ── LoadPlayerData ─────────────────────────────────────────────

type LoadPlayerDataReq struct {
	PlayerID uint64
	Fields   []string
}

type LoadPlayerDataResp struct {
	ErrorCode uint32
	Data      map[string][]byte
	Version   uint64
}

func LoadPlayerData(ctx context.Context, svcCtx *svc.ServiceContext, req *LoadPlayerDataReq) (*LoadPlayerDataResp, error) {
	client, err := svcCtx.Router.ClientForPlayer(ctx, req.PlayerID)
	if err != nil {
		return &LoadPlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
	}

	result := make(map[string][]byte)

	if len(req.Fields) == 0 {
		// Load all fields via SCAN with player prefix
		pattern := fmt.Sprintf("player:{%d}:*", req.PlayerID)
		var keys []string
		var cursor uint64
		for {
			var batch []string
			var err error
			batch, cursor, err = scan(ctx, client, cursor, pattern, 100)
			if err != nil {
				return &LoadPlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
			}
			keys = append(keys, batch...)
			if cursor == 0 {
				break
			}
		}

		if len(keys) > 0 {
			vals, err := mget(ctx, client, keys...)
			if err != nil {
				return &LoadPlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
			}
			prefix := fmt.Sprintf("player:{%d}:", req.PlayerID)
			for i, key := range keys {
				if vals[i] == nil {
					continue
				}
				fieldName := key[len(prefix):]
				if fieldName == "__version" {
					continue
				}
				result[fieldName] = []byte(vals[i].(string))
			}
		}
	} else {
		keys := make([]string, len(req.Fields))
		for i, f := range req.Fields {
			keys[i] = playerField(req.PlayerID, f)
		}
		vals, err := mget(ctx, client, keys...)
		if err != nil {
			return &LoadPlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
		}
		for i, f := range req.Fields {
			if vals[i] == nil {
				continue
			}
			result[f] = []byte(vals[i].(string))
		}
	}

	// Read version
	ver := readVersion(ctx, client, req.PlayerID)

	return &LoadPlayerDataResp{Data: result, Version: ver}, nil
}

// ── SavePlayerData ─────────────────────────────────────────────

type SavePlayerDataReq struct {
	PlayerID        uint64
	Data            map[string][]byte
	ExpectedVersion uint64
}

type SavePlayerDataResp struct {
	ErrorCode  uint32
	NewVersion uint64
}

func SavePlayerData(ctx context.Context, svcCtx *svc.ServiceContext, req *SavePlayerDataReq) (*SavePlayerDataResp, error) {
	client, err := svcCtx.Router.ClientForPlayer(ctx, req.PlayerID)
	if err != nil {
		return &SavePlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
	}

	// Acquire per-player lock
	locked, err := svcCtx.Router.AcquirePlayerLock(ctx, req.PlayerID)
	if err != nil {
		return &SavePlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
	}
	if !locked {
		logx.Errorf("player %d data is being written by another process", req.PlayerID)
		return &SavePlayerDataResp{ErrorCode: constants.ErrCodeLockConflict}, nil
	}
	defer svcCtx.Router.ReleasePlayerLock(ctx, req.PlayerID)

	// Version check
	if req.ExpectedVersion > 0 {
		curVer := readVersion(ctx, client, req.PlayerID)
		if curVer != req.ExpectedVersion {
			logx.Errorf("version mismatch for player %d: expected %d, got %d",
				req.PlayerID, req.ExpectedVersion, curVer)
			return &SavePlayerDataResp{ErrorCode: constants.ErrCodeVersionMismatch, NewVersion: curVer}, nil
		}
	}

	// Write fields via pipeline
	pipe := pipeline(client)
	for field, val := range req.Data {
		pipe.Set(ctx, playerField(req.PlayerID, field), val, 0)
	}
	// Increment version
	vk := versionKey(req.PlayerID)
	incrCmd := pipe.Incr(ctx, vk)
	_, err = pipe.Exec(ctx)
	if err != nil {
		return &SavePlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
	}

	newVer := uint64(incrCmd.Val())
	return &SavePlayerDataResp{NewVersion: newVer}, nil
}

// ── GetPlayerField ─────────────────────────────────────────────

func GetPlayerField(ctx context.Context, svcCtx *svc.ServiceContext, playerID uint64, field string) ([]byte, error) {
	client, err := svcCtx.Router.ClientForPlayer(ctx, playerID)
	if err != nil {
		return nil, err
	}
	val, err := getString(ctx, client, playerField(playerID, field))
	if err == goredis.Nil {
		return nil, nil
	}
	return []byte(val), err
}

// ── SetPlayerField ─────────────────────────────────────────────

type SetPlayerFieldResp struct {
	ErrorCode  uint32
	NewVersion uint64
}

func SetPlayerField(ctx context.Context, svcCtx *svc.ServiceContext, playerID uint64, field string, value []byte, expectedVersion uint64) (*SetPlayerFieldResp, error) {
	client, err := svcCtx.Router.ClientForPlayer(ctx, playerID)
	if err != nil {
		return &SetPlayerFieldResp{ErrorCode: constants.ErrCodeRedis}, err
	}

	locked, err := svcCtx.Router.AcquirePlayerLock(ctx, playerID)
	if err != nil {
		return &SetPlayerFieldResp{ErrorCode: constants.ErrCodeRedis}, err
	}
	if !locked {
		return &SetPlayerFieldResp{ErrorCode: constants.ErrCodeLockConflict}, nil
	}
	defer svcCtx.Router.ReleasePlayerLock(ctx, playerID)

	if expectedVersion > 0 {
		curVer := readVersion(ctx, client, playerID)
		if curVer != expectedVersion {
			return &SetPlayerFieldResp{ErrorCode: constants.ErrCodeVersionMismatch, NewVersion: curVer}, nil
		}
	}

	pipe := pipeline(client)
	pipe.Set(ctx, playerField(playerID, field), value, 0)
	incrCmd := pipe.Incr(ctx, versionKey(playerID))
	_, err = pipe.Exec(ctx)
	if err != nil {
		return &SetPlayerFieldResp{ErrorCode: constants.ErrCodeRedis}, err
	}

	return &SetPlayerFieldResp{NewVersion: uint64(incrCmd.Val())}, nil
}

// ── Helpers: abstract over Client vs ClusterClient ─────────────

func readVersion(ctx context.Context, client goredis.Cmdable, playerID uint64) uint64 {
	val, err := getString(ctx, client, versionKey(playerID))
	if err != nil || val == "" {
		return 0
	}
	v, _ := strconv.ParseUint(val, 10, 64)
	return v
}

func getString(ctx context.Context, c goredis.Cmdable, key string) (string, error) {
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()
	return c.Get(ctx, key).Result()
}

func mget(ctx context.Context, c goredis.Cmdable, keys ...string) ([]interface{}, error) {
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()
	return c.MGet(ctx, keys...).Result()
}

func scan(ctx context.Context, c goredis.Cmdable, cursor uint64, pattern string, count int64) ([]string, uint64, error) {
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()
	switch cc := c.(type) {
	case *goredis.Client:
		return cc.Scan(ctx, cursor, pattern, count).Result()
	case *goredis.ClusterClient:
		return cc.Scan(ctx, cursor, pattern, count).Result()
	default:
		return nil, 0, fmt.Errorf("unsupported redis client type")
	}
}

func pipeline(c goredis.Cmdable) goredis.Pipeliner {
	switch cc := c.(type) {
	case *goredis.Client:
		return cc.Pipeline()
	case *goredis.ClusterClient:
		return cc.Pipeline()
	default:
		panic("unsupported redis client type for pipeline")
	}
}

// ── DeletePlayerData ───────────────────────────────────────────

type DeletePlayerDataReq struct {
	PlayerID          uint64
	DeleteZoneMapping bool
}

type DeletePlayerDataResp struct {
	ErrorCode   uint32
	KeysDeleted uint32
}

func DeletePlayerData(ctx context.Context, svcCtx *svc.ServiceContext, req *DeletePlayerDataReq) (*DeletePlayerDataResp, error) {
	client, err := svcCtx.Router.ClientForPlayer(ctx, req.PlayerID)
	if err != nil {
		return &DeletePlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
	}

	locked, err := svcCtx.Router.AcquirePlayerLock(ctx, req.PlayerID)
	if err != nil {
		return &DeletePlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
	}
	if !locked {
		return &DeletePlayerDataResp{ErrorCode: constants.ErrCodeLockConflict}, nil
	}
	defer svcCtx.Router.ReleasePlayerLock(ctx, req.PlayerID)

	// Scan all keys for this player
	pattern := fmt.Sprintf("player:{%d}:*", req.PlayerID)
	var allKeys []string
	var cursor uint64
	for {
		batch, next, err := scan(ctx, client, cursor, pattern, 100)
		if err != nil {
			return &DeletePlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
		}
		allKeys = append(allKeys, batch...)
		cursor = next
		if cursor == 0 {
			break
		}
	}

	if len(allKeys) > 0 {
		err = del(ctx, client, allKeys...)
		if err != nil {
			return &DeletePlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
		}
	}

	deleted := uint32(len(allKeys))

	// Optionally remove zone mapping
	if req.DeleteZoneMapping {
		if err := svcCtx.Router.DeletePlayerZone(ctx, req.PlayerID); err != nil {
			logx.Errorf("failed to delete zone mapping for player %d: %v", req.PlayerID, err)
		} else {
			deleted++
		}
	}

	return &DeletePlayerDataResp{KeysDeleted: deleted}, nil
}

func del(ctx context.Context, c goredis.Cmdable, keys ...string) error {
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()
	return c.Del(ctx, keys...).Err()
}
