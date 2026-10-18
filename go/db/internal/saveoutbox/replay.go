package saveoutbox

import (
	"context"
	"errors"
	"fmt"
	"strconv"
	"time"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"shared/kafkautil"
	"shared/placement"
	"shared/savebarrier"
)

// Publisher 只有 broker acks=all 确认才可回 nil；结果未知也可晚到，调用者仍保留原意图重投。
type Publisher interface {
	Publish(context.Context, string, string, []byte) error
}

type Config struct {
	ZoneID          uint32
	TopicGeneration uint32
	// 每拍只读一个确定的桶；最多Batch个玩家，总预算Budget，不全量SCAN玩家键。
	Batch      int
	Interval   time.Duration
	Budget     time.Duration
	RetryAfter time.Duration
}

func (c Config) normalized() (Config, error) {
	if !placement.IsZoneStorage(c.ZoneID) {
		return c, errors.New("save outbox zone 不合法")
	}
	if c.TopicGeneration == 0 {
		c.TopicGeneration = 1
	}
	if c.Batch == 0 {
		c.Batch = 16
	}
	if c.Interval == 0 {
		c.Interval = 100 * time.Millisecond
	}
	if c.Budget == 0 {
		c.Budget = 2 * time.Second
	}
	if c.RetryAfter == 0 {
		c.RetryAfter = 5 * time.Second
	}
	if c.Batch < 1 || c.Batch > 64 || c.Interval < 10*time.Millisecond || c.Interval > time.Second ||
		c.Budget < time.Millisecond || c.Budget > 10*time.Second || c.RetryAfter <= c.Budget || c.RetryAfter > time.Minute {
		return c, errors.New("save outbox 有界轮询参数不合法")
	}
	return c, nil
}

// Replay 的桶游标仅由 Run/Tick 的单个调用者拥有；不能并发调用 Tick。
// 进程/副本间通过Redis短租期分工，所有投递仍由既有SQL围栏幂等。
type Replay struct {
	rc      redis.Cmdable
	mapping redis.Cmdable
	pub     Publisher
	cfg     Config
	now     func() time.Time
	bucket  uint64
	results *prometheus.CounterVec
}

func New(rc, mapping redis.Cmdable, pub Publisher, cfg Config, reg prometheus.Registerer) (*Replay, error) {
	if rc == nil || mapping == nil || pub == nil {
		return nil, errors.New("save outbox Redis/publisher 依赖缺失")
	}
	cfg, err := cfg.normalized()
	if err != nil {
		return nil, err
	}
	if reg == nil {
		reg = prometheus.DefaultRegisterer
	}
	counter := prometheus.NewCounterVec(prometheus.CounterOpts{Name: "db_save_outbox_total",
		Help: "玩家持久存盘意图补投结果；published仅指broker确认，completed要求两表SQL屏障ACK。"}, []string{"result"})
	if err := reg.Register(counter); err != nil {
		if existing, ok := err.(prometheus.AlreadyRegisteredError); ok {
			var same bool
			counter, same = existing.ExistingCollector.(*prometheus.CounterVec)
			if !same {
				return nil, errors.New("save outbox 指标类型冲突")
			}
		} else {
			return nil, err
		}
	}
	for _, label := range []string{"published", "completed", "migrated", "conflict", "invalid", "redis_error", "publish_error"} {
		counter.WithLabelValues(label)
	}
	return &Replay{rc: rc, mapping: mapping, pub: pub, cfg: cfg, now: time.Now, results: counter}, nil
}

func (r *Replay) Run(ctx context.Context) {
	ticker := time.NewTicker(r.cfg.Interval)
	defer ticker.Stop()
	for {
		if _, err := r.Tick(ctx); err != nil && ctx.Err() == nil {
			logx.Errorf("[SaveOutbox] 本拍补投未完成，原意图保留: %v", err)
		}
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
		}
	}
}

// Tick 返回本拍成功broker确认的DBTask条数。任何失败都保留意图，不根据Kafka ACK清理。
func (r *Replay) Tick(ctx context.Context) (int, error) {
	ctx, cancel := context.WithTimeout(ctx, r.cfg.Budget)
	defer cancel()
	bucket := r.bucket
	r.bucket = (r.bucket + 1) % BucketCount
	index := pendingIndexKey(r.cfg.ZoneID, bucket)
	now := r.now().UnixMilli()
	if now <= 0 {
		return 0, errors.New("save outbox 时钟不合法")
	}
	ids, err := r.rc.ZRangeByScore(ctx, index, &redis.ZRangeBy{Min: "-inf", Max: strconv.FormatInt(now, 10), Count: int64(r.cfg.Batch)}).Result()
	if err != nil {
		r.results.WithLabelValues("redis_error").Inc()
		return 0, errors.New("save outbox 读取桶索引失败")
	}
	published := 0
	for _, id := range ids {
		if err := ctx.Err(); err != nil {
			return published, err
		}
		playerID, err := strconv.ParseUint(id, 10, 64)
		if err != nil || playerID == 0 || kafkautil.DBTaskKey(playerID) != id || playerID%BucketCount != bucket {
			r.results.WithLabelValues("invalid").Inc()
			// 坏member也推迟，不能让一页坏数据永远饿死后续玩家；不删除证据。
			if err := postponeInvalidMemberScript.Run(ctx, r.rc, []string{index}, id, now, now+r.cfg.RetryAfter.Milliseconds()).Err(); err != nil {
				return published, errors.New("save outbox 推迟坏索引失败")
			}
			continue
		}
		reply, err := claimScript.Run(ctx, r.rc, []string{savebarrier.Key(playerID), index}, id, now, now+r.cfg.RetryAfter.Milliseconds()).Slice()
		if err != nil {
			r.results.WithLabelValues("redis_error").Inc()
			return published, errors.New("save outbox 原子认领失败")
		}
		if len(reply) == 0 {
			return published, errors.New("save outbox 认领返回为空")
		}
		code, ok := reply[0].(int64)
		if !ok {
			return published, errors.New("save outbox 认领返回非法")
		}
		switch code {
		case 0:
			continue
		case 2:
			r.results.WithLabelValues("completed").Inc()
			continue
		case -1:
			r.results.WithLabelValues("invalid").Inc()
			logx.Errorf("[SaveOutbox] 存盘意图缺失或损坏，保留待人工核对")
			continue
		case 1:
		default:
			return published, fmt.Errorf("save outbox 未知认领结局 %d", code)
		}
		in, err := decodeIntent(reply)
		if err != nil {
			r.results.WithLabelValues("invalid").Inc()
			continue
		}
		zone, err := validateIntent(playerID, in)
		if err != nil {
			r.results.WithLabelValues("invalid").Inc()
			logx.Errorf("[SaveOutbox] 存盘意图身份不合法，保留待核对: %v", err)
			continue
		}
		if in.topic != topicFor(zone, r.cfg.TopicGeneration) {
			r.results.WithLabelValues("invalid").Inc()
			logx.Errorf("[SaveOutbox] 原topic世代未在当前配置放行，保留意图；切换世代前必须排空")
			continue
		}
		if zone != r.cfg.ZoneID {
			home, err := r.mapping.Get(ctx, placement.HomeZoneKey(playerID)).Result()
			if err != nil || home != strconv.FormatUint(uint64(zone), 10) {
				r.results.WithLabelValues("invalid").Inc()
				logx.Errorf("[SaveOutbox] 跨zone索引迁移缺少匹配的权威home_zone，原意图保留")
				continue
			}
			moved, err := migrateScript.Run(ctx, r.rc, []string{savebarrier.Key(playerID), index, pendingIndexKey(zone, bucket)}, id, in.version, in.topic).Int64()
			if err != nil {
				r.results.WithLabelValues("redis_error").Inc()
				return published, errors.New("save outbox 迁移旧zone索引失败")
			}
			label := "conflict"
			if moved == 1 {
				label = "migrated"
			}
			r.results.WithLabelValues(label).Inc()
			continue
		}
		for _, payload := range []string{in.first, in.second} {
			if err := r.pub.Publish(ctx, in.topic, id, []byte(payload)); err != nil {
				r.results.WithLabelValues("publish_error").Inc()
				return published, errors.New("save outbox Kafka确认失败，原任务保留")
			}
			published++
			r.results.WithLabelValues("published").Inc()
		}
	}
	return published, nil
}

func decodeIntent(reply []interface{}) (intent, error) {
	if len(reply) != 5 {
		return intent{}, errors.New("存盘意图返回字段不完整")
	}
	fields := make([]string, 4)
	for i := range fields {
		var ok bool
		fields[i], ok = reply[i+1].(string)
		if !ok {
			return intent{}, errors.New("存盘意图返回字段类型不合法")
		}
	}
	return intent{version: fields[0], topic: fields[1], first: fields[2], second: fields[3]}, nil
}
