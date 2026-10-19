package debugutil

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

// ReadRedisKeys 在**一次 MULTI/EXEC** 里读完 specs 的类型、TTL 与值,得到同一时刻的一致视图
// (父快照、owner_epoch、屏障不会来自两次不同的存盘)。只发读命令;不 SCAN、不 KEYS。
//
// 每把键按 spec.Kind 只发一条取值命令。实际类型与期望不符时,那条命令在事务里单独报 WRONGTYPE,
// 不影响其他键;结果里记下实际类型与错误。EXEC 本身失败(连接断开、指向 Cluster 跨槽)时整体返回错误。
func ReadRedisKeys(ctx context.Context, client redis.Cmdable, specs []RedisKeySpec) ([]RedisEntry, error) {
	if len(specs) == 0 {
		return nil, nil
	}
	type pending struct {
		typ   *redis.StatusCmd
		pttl  *redis.DurationCmd
		str   *redis.StringCmd
		hash  *redis.MapStringStringCmd
		score *redis.FloatCmd
	}
	reads := make([]pending, len(specs))
	_, execErr := client.TxPipelined(ctx, func(pipe redis.Pipeliner) error {
		for i, spec := range specs {
			reads[i].typ = pipe.Type(ctx, spec.Key)
			reads[i].pttl = pipe.PTTL(ctx, spec.Key)
			switch spec.Kind {
			case RedisKindString:
				reads[i].str = pipe.Get(ctx, spec.Key)
			case RedisKindHash:
				reads[i].hash = pipe.HGetAll(ctx, spec.Key)
			case RedisKindZSetMember:
				reads[i].score = pipe.ZScore(ctx, spec.Key, spec.Member)
			}
		}
		return nil
	})

	out := make([]RedisEntry, len(specs))
	for i, spec := range specs {
		entry := RedisEntry{Key: spec.Key, Instance: spec.Instance, Purpose: spec.Purpose, Member: spec.Member}
		typ, err := reads[i].typ.Result()
		if err != nil {
			// TYPE 不会因键的类型失败;它失败说明整个事务没执行。
			if execErr == nil {
				execErr = err
			}
			return nil, fmt.Errorf("redis MULTI 读取失败(键 %s): %w", spec.Key, execErr)
		}
		entry.Type = typ
		entry.PTTLMs = durationMs(reads[i].pttl.Val())
		if typ == "none" {
			out[i] = entry
			continue
		}
		switch spec.Kind {
		case RedisKindString:
			value, err := reads[i].str.Bytes()
			if err != nil {
				entry.Error = fmt.Sprintf("期望 string,实际 %s: %v", typ, err)
				break
			}
			rendered := renderRedisValue(value, spec)
			entry.Value = &rendered
		case RedisKindHash:
			fields, err := reads[i].hash.Result()
			if err != nil {
				entry.Error = fmt.Sprintf("期望 hash,实际 %s: %v", typ, err)
				break
			}
			entry.Hash = make(map[string]Value, len(fields))
			for name, raw := range fields {
				if decode, ok := spec.HashDecode[name]; ok {
					entry.Hash[name] = RenderProto([]byte(raw), decode, nil)
					continue
				}
				entry.Hash[name] = RenderText([]byte(raw))
			}
		case RedisKindZSetMember:
			_, err := reads[i].score.Result()
			switch {
			case err == nil:
				entry.MemberPresent = true
			case errors.Is(err, redis.Nil):
			default:
				entry.Error = fmt.Sprintf("期望 zset,实际 %s: %v", typ, err)
			}
		}
		out[i] = entry
	}
	return out, nil
}

func renderRedisValue(value []byte, spec RedisKeySpec) Value {
	if spec.Decode != nil {
		return RenderProto(value, spec.Decode, spec.Redact)
	}
	return RenderText(value)
}

// durationMs 把 go-redis 的 PTTL 结果换成毫秒;-1(无 TTL)/ -2(不存在)原样保留。
func durationMs(d time.Duration) int64 {
	if d < 0 {
		return int64(d)
	}
	return d.Milliseconds()
}

// FindRedisEntry 按键名找结果;没有时返回 nil。
func FindRedisEntry(entries []RedisEntry, key string) *RedisEntry {
	for i := range entries {
		if entries[i].Key == key {
			return &entries[i]
		}
	}
	return nil
}

// EntryText 返回 string 键的文本值;键不存在或不是文本时返回空串。
func EntryText(entry *RedisEntry) string {
	if entry == nil || entry.Value == nil {
		return ""
	}
	return entry.Value.Text
}

// HashText 返回 hash 键某个字段的文本值。
func HashText(entry *RedisEntry, field string) string {
	if entry == nil || entry.Hash == nil {
		return ""
	}
	value := entry.Hash[field]
	return value.Text
}
