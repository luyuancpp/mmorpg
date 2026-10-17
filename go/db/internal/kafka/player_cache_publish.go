package kafka

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
	"google.golang.org/protobuf/proto"
)

// 单键 CAS 保留现有 protobuf 缓存格式，兼容旧读者，也不引入可能与值分离丢失的版本键。
const playerCacheCASScript = `
local current = redis.call('GET', KEYS[1])
if ARGV[1] == '0' then
  if current then return 0 end
elseif not current or current ~= ARGV[2] then return 0 end
redis.call('SET', KEYS[1], ARGV[3], 'PX', ARGV[4])
return 1`

func publishPlayerCache(ctx context.Context, client redis.Cmdable, key string, msg proto.Message, data []byte, ttl time.Duration) (bool, error) {
	desc := msg.ProtoReflect().Descriptor()
	if desc.FullName() != "player_database" && desc.FullName() != "player_database_1" {
		return false, nil
	}
	epochField, seqField := desc.Fields().ByName("fence_owner_epoch"), desc.Fields().ByName("fence_save_seq")
	idField := desc.Fields().ByName("player_id")
	if epochField == nil || seqField == nil || idField == nil {
		return true, errors.New("玩家缓存围栏字段缺失")
	}
	epoch, seq := msg.ProtoReflect().Get(epochField).Uint(), msg.ProtoReflect().Get(seqField).Uint()
	for attempt := 0; attempt < 3; attempt++ {
		current, err := client.Get(ctx, key).Bytes()
		exists := "1"
		if errors.Is(err, redis.Nil) {
			exists = "0"
			current = nil
		} else if err != nil {
			return true, err
		}
		if exists == "1" {
			old := msg.ProtoReflect().Type().New().Interface()
			if err := proto.Unmarshal(current, old); err != nil {
				return true, fmt.Errorf("现有玩家缓存损坏: %w", err)
			}
			if old.ProtoReflect().Get(idField).Uint() != msg.ProtoReflect().Get(idField).Uint() {
				return true, errors.New("玩家缓存身份冲突")
			}
			oldEpoch, oldSeq := old.ProtoReflect().Get(epochField).Uint(), old.ProtoReflect().Get(seqField).Uint()
			if oldEpoch > epoch || oldEpoch == epoch && oldSeq > seq {
				return true, nil
			}
			if epoch != 0 && seq != 0 && oldEpoch == epoch && oldSeq == seq {
				if !proto.Equal(old, msg) {
					return true, errors.New("玩家缓存同版本内容冲突")
				}
				return true, nil
			}
		}
		n, err := client.Eval(ctx, playerCacheCASScript, []string{key}, exists, current, data, ttl.Milliseconds()).Int64()
		if err != nil {
			return true, err
		}
		if n == 1 {
			return true, nil
		}
	}
	return true, errors.New("玩家缓存并发版本变化，保留待办重试")
}
