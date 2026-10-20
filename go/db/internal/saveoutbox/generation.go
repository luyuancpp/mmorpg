package saveoutbox

import (
	"context"
	"errors"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
)

// 存盘协议世代门禁(data-overwrite-rollback-audit-20261018.md §2.2，db-write-fencing-mysql.md 2026-10-18 附注)。
//
// 现行 C++ scene(owner_epoch≠0)与 login 只说严格提交屏障：父快照原子写 pending 与完整投递意图，login 冷加载与跨场景交接
// 要求两表 SQL ACK 都等于 pending。只有 Fencing.Mode=dual 的 go/db 写 ACK、跑补投。legacy go/db 混进这样的 zone，
// 玩家第二次登录起全部失败，交接只能等冻结上限；与 dual 混跑时，补投的旧意图还会被 legacy 实例无条件写进 MySQL。
//
// 世代由父缓存所在的同一 Redis 实例上的两类证据机器判定，任一成立即为严格世代：
//   - GenerationKey(zone) 存在：dual go/db 启动时写入，不设 TTL，只由运维按设计文档 §8.6 的降级流程删除；
//   - 本 zone 64 个补投桶任一非空：严格写者已经留下 legacy 既不能 ACK 也不能补投的意图。
// legacy 实例启动前检查一次，运行中每 LegacyWatchInterval 复查一次；复查期间续写 legacyAliveKey，dual 的补投在它存在时
// 整拍跳过，等 legacy 实例退出、标记过期后再投。

// GenerationStrict 是唯一已定义的世代值；读到其他任何值都按未知世代拒绝，不猜测。
const GenerationStrict = "strict"

// LegacyWatchInterval 是 legacy 实例复查世代与续写存活标记的周期；legacyAliveTTL 必须明显大于它。
const LegacyWatchInterval = 5 * time.Second

const legacyAliveTTL = 30 * time.Second

// ErrStrictGeneration 表示本 zone 已是严格提交屏障世代，legacy go/db 不得启动或继续运行。
var ErrStrictGeneration = errors.New("本 zone 已是严格存盘提交屏障世代，legacy go/db 不能为它写 SQL ACK 或补投")

// GenerationKey 是 zone 级世代标记。与父缓存、save_barrier、补投桶同一 Redis 实例，判定与 C++ 写入看到的是同一份事实。
func GenerationKey(zone uint32) string {
	return fmt.Sprintf("db:save_protocol:zone:%d", zone)
}

// legacyAliveKey 由 legacy 实例续写(PX legacyAliveTTL)；存在即说明本 zone 可能还有不做围栏的写者在消费。
func legacyAliveKey(zone uint32) string {
	return fmt.Sprintf("db:save_protocol:legacy_alive:zone:%d", zone)
}

// 类型与取值先验后写；标记已存在且是未知世代时拒绝，不覆盖。SET 不带 PX，顺带清掉人为加上的 TTL。
var announceStrictScript = redis.NewScript(`
local t = redis.call('TYPE', KEYS[1]).ok
if t ~= 'none' and t ~= 'string' then return redis.error_reply('save protocol marker has wrong type') end
local cur = redis.call('GET', KEYS[1])
if cur and cur ~= ARGV[1] then return redis.error_reply('save protocol marker holds an unknown generation') end
redis.call('SET', KEYS[1], ARGV[1])
return 1
`)

// AnnounceStrictGeneration 由 dual 实例在启动消费者与补投之前调用。失败即拒启：没有标记时 legacy 实例无从得知本 zone 已切世代。
func AnnounceStrictGeneration(ctx context.Context, rc redis.Cmdable, zone uint32) error {
	if rc == nil || zone == 0 {
		return errors.New("存盘协议世代标记缺少 Redis 或 zone")
	}
	if err := announceStrictScript.Run(ctx, rc, []string{GenerationKey(zone)}, GenerationStrict).Err(); err != nil {
		return fmt.Errorf("写存盘协议世代标记 %s 失败: %w", GenerationKey(zone), err)
	}
	return nil
}

// CheckLegacyAllowed 判定本 zone 能否运行 legacy go/db。Redis 出错、键类型不符一律返回错误(fail-closed)，
// 严格世代证据返回包着 ErrStrictGeneration 的错误。
func CheckLegacyAllowed(ctx context.Context, rc redis.Cmdable, zone uint32) error {
	if rc == nil || zone == 0 {
		return errors.New("存盘协议世代判定缺少 Redis 或 zone")
	}
	marker, err := rc.Get(ctx, GenerationKey(zone)).Result()
	switch {
	case errors.Is(err, redis.Nil):
	case err != nil:
		return fmt.Errorf("读取存盘协议世代标记 %s 失败: %w", GenerationKey(zone), err)
	default:
		return fmt.Errorf("%w: %s=%q", ErrStrictGeneration, GenerationKey(zone), marker)
	}
	pipe := rc.Pipeline()
	counts := make([]*redis.IntCmd, BucketCount)
	for bucket := uint64(0); bucket < BucketCount; bucket++ {
		counts[bucket] = pipe.ZCard(ctx, pendingIndexKey(zone, bucket))
	}
	if _, err := pipe.Exec(ctx); err != nil {
		return fmt.Errorf("读取本 zone 存盘补投桶失败，不能证明没有严格写者: %w", err)
	}
	for bucket, count := range counts {
		if count.Val() > 0 {
			return fmt.Errorf("%w: %s 有 %d 个未取得两表 SQL ACK 的意图", ErrStrictGeneration, pendingIndexKey(zone, uint64(bucket)), count.Val())
		}
	}
	return nil
}

// WatchLegacy 在 legacy 实例运行期间周期复查世代并续写存活标记。发现严格世代证据时调用一次 onConflict
// (调用方据此走正常停机)，之后只续写存活标记、不再复查，直到 ctx 取消：停机排空期间实例仍可能在落库，补投必须继续等。
// Redis 暂时出错只记日志：此时本实例的存盘本身也在失败，复查留到下一拍。
func WatchLegacy(ctx context.Context, rc redis.Cmdable, zone uint32, interval time.Duration, onConflict func(error)) {
	if interval <= 0 || interval*2 >= legacyAliveTTL {
		logx.Errorf("[SaveProtocol] legacy复查周期 %v 不合法(须为正且小于存活标记TTL %v 的一半)，改用 %v", interval, legacyAliveTTL, LegacyWatchInterval)
		interval = LegacyWatchInterval
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	w := &legacyWatcher{rc: rc, zone: zone}
	for {
		stepCtx, cancel := context.WithTimeout(ctx, interval)
		conflict := w.step(stepCtx)
		cancel()
		if conflict != nil {
			onConflict(conflict)
		}
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
		}
	}
}

// legacyWatcher 是 WatchLegacy 的一拍，单独拆出来便于不依赖墙钟地测试。只由 WatchLegacy 的协程使用。
type legacyWatcher struct {
	rc         redis.Cmdable
	zone       uint32
	conflicted bool
}

// step 先续写存活标记，再复查世代；只在第一次发现严格世代证据时返回该错误，之后只续写。
func (w *legacyWatcher) step(ctx context.Context) error {
	if err := w.rc.Set(ctx, legacyAliveKey(w.zone), "1", legacyAliveTTL).Err(); err != nil && ctx.Err() == nil {
		logx.Errorf("[SaveProtocol] 续写legacy存活标记失败，dual补投可能提前恢复: %v", err)
	}
	if w.conflicted {
		return nil
	}
	err := CheckLegacyAllowed(ctx, w.rc, w.zone)
	switch {
	case errors.Is(err, ErrStrictGeneration):
		w.conflicted = true
		return err
	case err != nil && ctx.Err() == nil:
		logx.Errorf("[SaveProtocol] legacy世代复查失败，下一拍重试: %v", err)
	}
	return nil
}

// legacyAlive 报告本 zone 是否仍有 legacy 实例在续写存活标记。
func legacyAlive(ctx context.Context, rc redis.Cmdable, zone uint32) (bool, error) {
	n, err := rc.Exists(ctx, legacyAliveKey(zone)).Result()
	return n > 0, err
}
