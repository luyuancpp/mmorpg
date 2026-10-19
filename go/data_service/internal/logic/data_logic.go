package logic

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"time"

	"data_service/internal/constants"
	"data_service/internal/metrics"
	"data_service/internal/routing"
	"data_service/internal/svc"

	goredis "github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

const (
	playerDataPrefix = "player:{%d}:" // hash-tag ensures same slot
	playerVersionKey = "player:{%d}:__version"
)

func playerField(playerID uint64, field string) string {
	return fmt.Sprintf("player:{%d}:%s", playerID, field)
}

func versionKey(playerID uint64) string {
	return fmt.Sprintf(playerVersionKey, playerID)
}

func isInternalPlayerField(field string) bool {
	return field == "__version" || field == "__lock"
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

// LoadPlayerData 读玩家字段与乐观锁版本。
//
// 契约:返回的 Version 永远不比 Data 新。调用方拿它(非 0)做 CAS 存盘时,只要读之后有别的写落地,
// 存盘必然被拒;最坏只是多一次伪冲突,不会覆盖别人的写。版本键不存在回 0 —— 按 saveFieldsScript
// 的既有约定 expected_version=0 跳过校验,首次存盘前的并发不受本契约保护。任何读失败
// (含版本读失败、版本值损坏)一律 fail-closed 回 ErrCodeRedis —— 回一个 Version=0 的"成功"
// 会让调用方以为可以跳过版本校验(见 readVersion 注释),等于关掉乐观锁。
func LoadPlayerData(ctx context.Context, svcCtx *svc.ServiceContext, req *LoadPlayerDataReq) (*LoadPlayerDataResp, error) {
	for _, field := range req.Fields {
		if isInternalPlayerField(field) {
			return &LoadPlayerDataResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
		}
	}
	client, err := svcCtx.Router.ClientForPlayer(ctx, req.PlayerID)
	if err != nil {
		return &LoadPlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
	}

	var data map[string][]byte
	var ver uint64
	if len(req.Fields) == 0 {
		data, ver, err = loadAllPlayerFields(ctx, client, req.PlayerID)
	} else {
		data, ver, err = loadSelectedPlayerFields(ctx, client, req.PlayerID, req.Fields)
	}
	if err != nil {
		return &LoadPlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
	}
	return &LoadPlayerDataResp{Data: data, Version: ver}, nil
}

// loadSelectedPlayerFields 把版本键与字段键放进同一条 MGET:所有 player:{id}:* 共享 {id} hash-tag,
// 同一 slot,单条命令原子,读到的版本与字段属于同一瞬间。
//
// 旧写法先 MGET 字段、再单独 GET 版本,两步之间落地的 saveFieldsScript 会让返回值变成
// 「旧数据 + 新版本」,调用方据此 CAS 存盘能通过,覆盖掉中间那次写。
func loadSelectedPlayerFields(ctx context.Context, client goredis.Cmdable, playerID uint64, fields []string) (map[string][]byte, uint64, error) {
	keys := make([]string, 0, len(fields)+1)
	for _, f := range fields {
		keys = append(keys, playerField(playerID, f))
	}
	keys = append(keys, versionKey(playerID))
	vals, err := mget(ctx, client, keys...)
	if err != nil {
		return nil, 0, err
	}
	ver, err := parseVersionReply(playerID, vals[len(fields)])
	if err != nil {
		return nil, 0, err
	}
	data := make(map[string][]byte, len(fields))
	for i, f := range fields {
		if vals[i] == nil {
			continue
		}
		data[f] = []byte(vals[i].(string))
	}
	return data, ver, nil
}

// loadAllPlayerFields 用 SCAN 枚举字段键,再一条 MGET 连同版本键一起读。
//
// MGET 本身原子,但 SCAN 与 MGET 是两步:窗口内落地的存盘可能新建了 SCAN 没扫到的字段键,
// 这时 MGET 里的版本对应的数据并不完整。拿它做 CAS 会通过,调用方若据此「新建」那个它没看到的字段,
// 就覆盖了窗口里的那次写。所以 SCAN 之前先读一次版本(先读版本、再读数据):
//   - MGET 里的版本与之相同 → 窗口内没有存盘(saveFieldsScript 成功必 INCR),SCAN 的键集完整,
//     数据与版本属于同一瞬间;
//   - 不同 → 返回 SCAN 之前的版本。它比数据旧,调用方据此 CAS 必然被拒,只多一次伪冲突。
func loadAllPlayerFields(ctx context.Context, client goredis.Cmdable, playerID uint64) (map[string][]byte, uint64, error) {
	verBeforeScan, err := readVersion(ctx, client, playerID)
	if err != nil {
		return nil, 0, err
	}

	pattern := fmt.Sprintf("player:{%d}:*", playerID)
	var keys []string
	var cursor uint64
	for {
		var batch []string
		batch, cursor, err = scan(ctx, client, cursor, pattern, 100)
		if err != nil {
			return nil, 0, err
		}
		keys = append(keys, batch...)
		if cursor == 0 {
			break
		}
	}

	// 版本键固定放在末尾:SCAN 扫没扫到它都一样(重复的键 MGET 照常返回,扫到的那份下面按内部字段跳过)。
	keys = append(keys, versionKey(playerID))
	vals, err := mget(ctx, client, keys...)
	if err != nil {
		return nil, 0, err
	}
	verAtRead, err := parseVersionReply(playerID, vals[len(vals)-1])
	if err != nil {
		return nil, 0, err
	}

	prefix := fmt.Sprintf(playerDataPrefix, playerID)
	data := make(map[string][]byte)
	for i, key := range keys[:len(keys)-1] {
		if vals[i] == nil {
			continue
		}
		fieldName := key[len(prefix):]
		if isInternalPlayerField(fieldName) {
			continue
		}
		data[fieldName] = []byte(vals[i].(string))
	}

	if verAtRead != verBeforeScan {
		logx.Infof("[LoadPlayerData] player %d: save landed during full load (version %d -> %d), returning the pre-scan version",
			playerID, verBeforeScan, verAtRead)
		return data, verBeforeScan, nil
	}
	return data, verAtRead, nil
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

// acquirePlayerLock acquires a per-player lock, returning the Redis client for
// that player plus the lock ownership token. On failure it returns a non-nil
// error code. Caller must defer svcCtx.Router.ReleasePlayerLock(ctx, client, id, token);
// 直接传请求 ctx 即可:释放自带不随请求取消的短超时(见 routing.ReleasePlayerLock)。
func acquirePlayerLock(ctx context.Context, svcCtx *svc.ServiceContext, playerID uint64) (goredis.Cmdable, string, uint32, error) {
	client, err := svcCtx.Router.ClientForPlayer(ctx, playerID)
	if err != nil {
		metrics.ObservePlayerLock("error")
		return nil, "", constants.ErrCodeRedis, err
	}
	token, locked, err := svcCtx.Router.AcquirePlayerLock(ctx, client, playerID)
	if err != nil {
		metrics.ObservePlayerLock("error")
		return nil, "", constants.ErrCodeRedis, err
	}
	if !locked {
		metrics.ObservePlayerLock("conflict")
		logx.Errorf("player %d data is being written by another process", playerID)
		return nil, "", constants.ErrCodeLockConflict, nil
	}
	metrics.ObservePlayerLock("acquired")
	return client, token, 0, nil
}

// saveFieldsScript 原子完成「版本比较 → 字段写入 → 版本自增」。
//
// 拆开做(先 GET 比版本、再 Pipeline 写)会留一条竞态:两个写者在对方写入前
// 都读到相同版本、双双通过检查,随后交错写入 —— 乐观锁失效,玩家数据出现
// 字段级撕裂。玩家锁挡不住这条路:它带 TTL,慢写者超时后锁会被下一个写者
// 合法拿走。所有 player:{id}:* key 共享 {id} hash-tag(见 playerDataPrefix),
// 同一 slot,Cluster 下单脚本可覆盖。
//
// KEYS[1] = data lock key,KEYS[2] = version key,KEYS[3..n] = field keys;
// ARGV[1] = lock token,ARGV[2] = expected_version(0 = 跳过检查),
// ARGV[3..n] = 字段值,与 KEYS 同序对应。
// 返回 {1, new_version}、{0, current_version}(版本冲突)或 {-1, current_version}(锁已失效)。
const saveFieldsScript = `
if redis.call('GET', KEYS[1]) ~= ARGV[1] then
	return {-1, tonumber(redis.call('GET', KEYS[2]) or '0')}
end
local expected = tonumber(ARGV[2])
if expected ~= 0 then
	local cur = tonumber(redis.call('GET', KEYS[2]) or '0')
	if cur ~= expected then
		return {0, cur}
	end
end
for i = 3, #KEYS do
	redis.call('SET', KEYS[i], ARGV[i])
end
return {1, redis.call('INCR', KEYS[2])}`

type saveFieldsStatus int64

const (
	saveFieldsLockLost        saveFieldsStatus = -1
	saveFieldsVersionMismatch saveFieldsStatus = 0
	saveFieldsOK              saveFieldsStatus = 1
)

// saveFieldsAtomic runs saveFieldsScript. status 精确区分版本冲突与锁失效；
// 后者意味着当前调用已经是 TTL 后恢复的旧持锁者，绝不能继续写。
func saveFieldsAtomic(ctx context.Context, client goredis.Cmdable, playerID uint64, token string, expectedVersion uint64, data map[string][]byte) (saveFieldsStatus, uint64, error) {
	keys := make([]string, 0, len(data)+2)
	argv := make([]interface{}, 0, len(data)+2)
	keys = append(keys, routing.PlayerDataLockKey(playerID), versionKey(playerID))
	argv = append(argv, token, expectedVersion)
	for field, val := range data {
		keys = append(keys, playerField(playerID, field))
		argv = append(argv, val)
	}

	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()
	res, err := client.Eval(ctx, saveFieldsScript, keys, argv...).Result()
	if err != nil {
		return saveFieldsLockLost, 0, err
	}
	arr, ok := res.([]interface{})
	if !ok || len(arr) != 2 {
		return saveFieldsLockLost, 0, fmt.Errorf("unexpected saveFieldsScript reply: %T %v", res, res)
	}
	status, statusOK := arr[0].(int64)
	ver, versionOK := arr[1].(int64)
	if !statusOK || !versionOK || ver < 0 {
		return saveFieldsLockLost, 0, fmt.Errorf("unexpected saveFieldsScript values: %T/%v %T/%v",
			arr[0], arr[0], arr[1], arr[1])
	}
	if status != int64(saveFieldsLockLost) && status != int64(saveFieldsVersionMismatch) && status != int64(saveFieldsOK) {
		return saveFieldsLockLost, 0, fmt.Errorf("unexpected saveFieldsScript status: %d", status)
	}
	return saveFieldsStatus(status), uint64(ver), nil
}

func SavePlayerData(ctx context.Context, svcCtx *svc.ServiceContext, req *SavePlayerDataReq) (*SavePlayerDataResp, error) {
	startTime := time.Now()
	for field := range req.Data {
		if isInternalPlayerField(field) {
			metrics.ObserveSavePlayerData("invalid_request", startTime)
			return &SavePlayerDataResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
		}
	}
	client, token, errCode, err := acquirePlayerLock(ctx, svcCtx, req.PlayerID)
	if errCode != 0 {
		outcome := "redis_error"
		if errCode == constants.ErrCodeLockConflict {
			outcome = "lock_conflict"
		}
		metrics.ObserveSavePlayerData(outcome, startTime)
		return &SavePlayerDataResp{ErrorCode: errCode}, err
	}
	defer svcCtx.Router.ReleasePlayerLock(ctx, client, req.PlayerID, token)

	status, ver, err := saveFieldsAtomic(ctx, client, req.PlayerID, token, req.ExpectedVersion, req.Data)
	if err != nil {
		metrics.ObserveSavePlayerData("redis_error", startTime)
		return &SavePlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
	}
	if status == saveFieldsLockLost {
		metrics.ObserveSavePlayerData("lock_conflict", startTime)
		logx.Errorf("player %d lock ownership expired before atomic save; stale writer fenced", req.PlayerID)
		return &SavePlayerDataResp{ErrorCode: constants.ErrCodeLockConflict, NewVersion: ver}, nil
	}
	if status == saveFieldsVersionMismatch {
		logx.Errorf("version mismatch for player %d: expected %d, got %d", req.PlayerID, req.ExpectedVersion, ver)
		metrics.ObserveVersionMismatch("save_player_data")
		metrics.ObserveSavePlayerData("version_mismatch", startTime)
		return &SavePlayerDataResp{ErrorCode: constants.ErrCodeVersionMismatch, NewVersion: ver}, nil
	}

	metrics.ObserveSavePlayerData("ok", startTime)
	return &SavePlayerDataResp{NewVersion: ver}, nil
}

// ── GetPlayerField ─────────────────────────────────────────────

func GetPlayerField(ctx context.Context, svcCtx *svc.ServiceContext, playerID uint64, field string) ([]byte, error) {
	if isInternalPlayerField(field) {
		return nil, fmt.Errorf("field %q is internal", field)
	}
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
	if isInternalPlayerField(field) {
		return &SetPlayerFieldResp{ErrorCode: constants.ErrCodeInvalidRequest}, nil
	}
	client, token, errCode, err := acquirePlayerLock(ctx, svcCtx, playerID)
	if errCode != 0 {
		return &SetPlayerFieldResp{ErrorCode: errCode}, err
	}
	defer svcCtx.Router.ReleasePlayerLock(ctx, client, playerID, token)

	status, ver, err := saveFieldsAtomic(ctx, client, playerID, token, expectedVersion, map[string][]byte{field: value})
	if err != nil {
		return &SetPlayerFieldResp{ErrorCode: constants.ErrCodeRedis}, err
	}
	if status == saveFieldsLockLost {
		return &SetPlayerFieldResp{ErrorCode: constants.ErrCodeLockConflict, NewVersion: ver}, nil
	}
	if status == saveFieldsVersionMismatch {
		return &SetPlayerFieldResp{ErrorCode: constants.ErrCodeVersionMismatch, NewVersion: ver}, nil
	}

	return &SetPlayerFieldResp{NewVersion: ver}, nil
}

// ── Redis helper interfaces ────────────────────────────────────
// go-redis Cmdable doesn't expose Scan/Pipeline; define minimal interfaces
// so callers don't need type switches.

type redisScanner interface {
	Scan(ctx context.Context, cursor uint64, match string, count int64) *goredis.ScanCmd
}

// ── Helpers ────────────────────────────────────────────────────

// readVersion 读乐观锁版本号。
//
// 必须把「版本 key 不存在」和「读失败」分开返回:saveFieldsScript 里
// expected_version==0 的语义是**跳过版本校验**(见该脚本 ARGV[2] 注释)。
// 旧写法把 Redis 读错误(超时、连接抖动、类型异常)一律降级成 0 并且不带错误,
// LoadPlayerData 又把它当成正常结果返回(同函数里其它读失败都会置 ErrCodeRedis),
// 于是调用方拿着 version=0 去存盘 —— 乐观锁被静默关掉,变成无条件覆盖。
// 两个 scene 节点并发存同一个玩家时,后到的那次会直接盖掉前一次,表现为随机回档。
// key 不存在(首次存盘)仍然合法返回 0。
func readVersion(ctx context.Context, client goredis.Cmdable, playerID uint64) (uint64, error) {
	val, err := getString(ctx, client, versionKey(playerID))
	if err != nil {
		if err == goredis.Nil {
			return 0, nil
		}
		return 0, err
	}
	return parseVersionValue(playerID, val)
}

// parseVersionReply 解读 MGET 里版本键那一格:nil = 键不存在 = 0,与 readVersion 的口径一致。
func parseVersionReply(playerID uint64, raw interface{}) (uint64, error) {
	if raw == nil {
		return 0, nil
	}
	val, ok := raw.(string)
	if !ok {
		return 0, fmt.Errorf("unexpected version reply for player %d: %T %v", playerID, raw, raw)
	}
	return parseVersionValue(playerID, val)
}

func parseVersionValue(playerID uint64, val string) (uint64, error) {
	if val == "" {
		return 0, nil
	}
	v, err := strconv.ParseUint(val, 10, 64)
	if err != nil {
		return 0, fmt.Errorf("corrupt version value %q for player %d: %w", val, playerID, err)
	}
	return v, nil
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
	s, ok := c.(redisScanner)
	if !ok {
		return nil, 0, fmt.Errorf("redis client %T does not support Scan", c)
	}
	return s.Scan(ctx, cursor, pattern, count).Result()
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

// ErrZoneMappingDeletionDisabled:DeletePlayerData 带 DeleteZoneMapping=true 的请求一律拒绝(W3,2026-10-12,
// docs/design/player-route-mapping-durability-proposal.md §4.3)。拒绝发生在**任何副作用之前**:
// 先删数据键、再拒删映射,会留下"数据没了、映射还在"的半截操作。
// 为什么停用:锁 token 在 mapping Redis、映射真源要落到全局库 player_route,删映射与持锁校验无法原子;
// 删号与落点的关系本来就待定(player-storage-placement.md)。仓库内没有生产调用方。
// server 层把它翻成 gRPC FailedPrecondition(DeletePlayerData 唯一不走 in-band 码的失败)。
var ErrZoneMappingDeletionDisabled = errors.New("DeletePlayerData: delete_zone_mapping is disabled (proposal §4.3, W3)")

func DeletePlayerData(ctx context.Context, svcCtx *svc.ServiceContext, req *DeletePlayerDataReq) (*DeletePlayerDataResp, error) {
	if req.DeleteZoneMapping {
		logx.Errorf("[DeletePlayerData] player %d: refused before any side effect: delete_zone_mapping is disabled (proposal §4.3, W3)", req.PlayerID)
		return &DeletePlayerDataResp{ErrorCode: constants.ErrCodeNotImplemented},
			fmt.Errorf("%w: player %d", ErrZoneMappingDeletionDisabled, req.PlayerID)
	}

	client, token, errCode, err := acquirePlayerLock(ctx, svcCtx, req.PlayerID)
	if errCode != 0 {
		return &DeletePlayerDataResp{ErrorCode: errCode}, err
	}
	defer svcCtx.Router.ReleasePlayerLock(ctx, client, req.PlayerID, token)

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

	// 锁键本身由 defer 的 token 校验释放，不能混入业务数据删除列表。
	lockKey := routing.PlayerDataLockKey(req.PlayerID)
	dataKeys := allKeys[:0]
	for _, key := range allKeys {
		if key != lockKey {
			dataKeys = append(dataKeys, key)
		}
	}
	allKeys = dataKeys

	if len(allKeys) > 0 {
		owned, deleteErr := deletePlayerKeysAtomic(ctx, client, lockKey, token, allKeys)
		if deleteErr != nil {
			err = deleteErr
			logx.Errorf("[DeletePlayerData] player %d: failed to delete %d Redis keys: %v", req.PlayerID, len(allKeys), err)
			return &DeletePlayerDataResp{ErrorCode: constants.ErrCodeRedis}, err
		}
		if !owned {
			logx.Errorf("[DeletePlayerData] player %d: lock expired before delete; stale deleter fenced", req.PlayerID)
			return &DeletePlayerDataResp{ErrorCode: constants.ErrCodeLockConflict}, nil
		}
	}

	deleted := uint32(len(allKeys))

	// 映射删除分支已停用(入口处就拒绝了 DeleteZoneMapping=true,见 ErrZoneMappingDeletionDisabled),这里只删数据键。
	logx.Infof("[DeletePlayerData] player %d: deleted=%d (zone mapping kept)", req.PlayerID, deleted)
	return &DeletePlayerDataResp{KeysDeleted: deleted}, nil
}

const deletePlayerKeysScript = `
if redis.call('GET', KEYS[1]) ~= ARGV[1] then
	return -1
end
if #KEYS == 1 then
	return 0
end
return redis.call('DEL', unpack(KEYS, 2, #KEYS))`

func deletePlayerKeysAtomic(ctx context.Context, c goredis.Cmdable, lockKey, token string, keys []string) (bool, error) {
	ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
	defer cancel()
	allKeys := make([]string, 0, len(keys)+1)
	allKeys = append(allKeys, lockKey)
	allKeys = append(allKeys, keys...)
	result, err := c.Eval(ctx, deletePlayerKeysScript, allKeys, token).Int64()
	if err != nil {
		return false, err
	}
	return result >= 0, nil
}
