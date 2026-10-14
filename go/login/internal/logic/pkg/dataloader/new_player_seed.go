package dataloader

import (
	"context"
	"fmt"
	login_proto_database "proto/common/database"
	"shared/ownerepoch"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

// newPlayerAllDataSkeleton 返回「这名玩家在库里一行都没有」时父键的完整内容:两张子表各只带 player_id。
//
// 两条路共用这一个构造,必须同源(docs/design/new-player-first-enter-no-cold-read.md §4.1):
//   - EnsurePlayerAllDataInRedisAsync 冷读时以它为底。go/db 查不到行时原样回显请求体
//     (go/db key_ordered_consumer.go 的 handleDBReadOp / publishDBReadResult),子表保持这份内容;
//   - SeedNewPlayerAllData 在建角时直接写它。
//
// 所以新号的种子与「冷读读空后组装出的父键」逐字段相同,首次入场之后的身份补齐(player_class_backfill.go)
// 与 scene 的新号初始化(InitPlayerFromAllData 的 registration_timestamp 分支)走的是同一条既有路径。
// 这里**不能**带身份字段或 registration_timestamp:前者有 backfill 负责,后者一旦非 0,scene 会跳过首次注册。
func newPlayerAllDataSkeleton(playerId uint64) *login_proto_database.PlayerAllData {
	return &login_proto_database.PlayerAllData{
		PlayerDatabaseData:   &login_proto_database.PlayerDatabase{PlayerId: playerId},
		PlayerDatabase_1Data: &login_proto_database.PlayerDatabase_1{PlayerId: playerId},
	}
}

// SeedResult 是 SeedNewPlayerAllData 的结论。零值 SeedError:漏判的分支落在最保守的「不知道写没写」上
// (写了也无害,见 seedNewPlayerScript)。
type SeedResult uint8

const (
	SeedError            SeedResult = iota // Redis 出错 / 回复不认识:种子是否落地未知
	SeedWritten                            // 父键原本不存在,已写入种子
	SeedIdempotent                         // 父键已是这份种子:同一条 EVAL 被 go-redis 重发,或这个 id 以前写过种子(成因见 seedNewPlayerScript)
	SeedParentExists                       // 父键已存在且不是这份种子:未写
	SeedOwnerEpochExists                   // player:{id}:owner_epoch 已存在(玩家被派入过场景):未写
)

// String 返回指标 label 值(login_create_player_seed_total{result}),集合封闭。
func (r SeedResult) String() string {
	switch r {
	case SeedWritten:
		return "written"
	case SeedIdempotent:
		return "idempotent"
	case SeedParentExists:
		return "parent_exists"
	case SeedOwnerEpochExists:
		return "owner_epoch_exists"
	default:
		return "error"
	}
}

// seedNewPlayerScript 写新号种子。KEYS = {PlayerAllData 父键, player:{id}:owner_epoch};
// ARGV = {种子字节, TTL 毫秒}。
//
// 只在两件事同时成立时写:
//   - owner_epoch 键不存在。scene_manager 每次把玩家派入场景都会 INCR 它、从不删除(shared/ownerepoch 包注释;C++ redis_client.h kSaveIfGuardLuaScript 的注释)。
//     它在 = 这名玩家进过场景,scene 的存档才是真相,种子绝不能落。这一条兜住「种子请求在网络里滞留,
//     落地时玩家早已入场并存过盘、父键又恰好被删(合服失效缓存)」这类交错;
//   - 父键不存在。存在就不覆盖;若它恰好就是这份种子,回 2 算成功。回 2 有两种成因:go-redis 把已在服务端执行过的
//     EVAL 又重发了一次;或者这个 id 以前铸出过、当时写的种子还在(id 复用)。建角调用点套了 1s 的 ctx 预算,
//     读超时(3s)之后的重发到不了,只剩连接层快速失败(EOF / 连接被重置)后的重发,所以 idempotent 稳态
//     应接近 0,持续出现要按 id 复用排查。
//
// 写入带 PX,与冷读组装的父键同一 TTL(playerAllDataParentTTL);scene 首次存盘的裸 SET 会去掉 TTL。
// 两把键必须在同一实例:C++ 存盘的 owner_epoch 守卫脚本本来就同时访问它们(redis_client.h kSaveIfGuardLuaScript)。
//
// 返回:1 已写;2 已是这份种子;0 父键已存在且不同;-1 owner_epoch 已存在。
var seedNewPlayerScript = redis.NewScript(`
if redis.call("EXISTS", KEYS[2]) == 1 then
    return -1
end
local cur = redis.call("GET", KEYS[1])
if cur == false then
    redis.call("SET", KEYS[1], ARGV[1], "PX", ARGV[2])
    return 1
end
if cur == ARGV[1] then
    return 2
end
return 0
`)

// SeedNewPlayerAllData 在建角时写新号的 PlayerAllData 父键种子(内容 = newPlayerAllDataSkeleton),
// 让首次 EnterGame 的 EnsurePlayerAllDataInRedisAsync 命中快路径、不再发 Kafka 读任务。
//
// 前提(调用方保证):playerId 是刚铸出、尚未对任何人可见的号段 id。号段 id 不复用,所以此刻该 id 在任何库里
// 都没有行,种子就是真值。本函数只写缓存,不碰数据库;rc 必须与预加载、scene 读写父键是同一个 Redis 实例
// (既有契约,cross-zone-scene-travel.md CZ-2)。
//
// err 只在结论为 SeedError 时非 nil。
func SeedNewPlayerAllData(ctx context.Context, rc *redis.Client, playerId uint64) (SeedResult, error) {
	if rc == nil {
		return SeedError, fmt.Errorf("seed new player %d: redis client not wired", playerId)
	}
	if playerId == 0 {
		return SeedError, fmt.Errorf("seed new player: player_id is 0")
	}
	parent := newPlayerAllDataSkeleton(playerId)
	data, err := proto.Marshal(parent)
	if err != nil {
		return SeedError, fmt.Errorf("marshal seed for player %d: %w", playerId, err)
	}
	keys := []string{buildParentKey(parent, playerId), ownerepoch.OwnerEpochKey(playerId)}
	res, err := seedNewPlayerScript.Run(ctx, rc, keys, data, playerAllDataParentTTL.Milliseconds()).Int()
	if err != nil {
		return SeedError, fmt.Errorf("seed new player %d: %w", playerId, err)
	}
	switch res {
	case 1:
		return SeedWritten, nil
	case 2:
		return SeedIdempotent, nil
	case 0:
		return SeedParentExists, nil
	case -1:
		return SeedOwnerEpochExists, nil
	default:
		return SeedError, fmt.Errorf("seed new player %d: unexpected script result %d", playerId, res)
	}
}
