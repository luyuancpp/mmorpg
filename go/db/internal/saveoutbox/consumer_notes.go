package saveoutbox

import (
	"context"
	"errors"
	"strconv"
	"time"

	"github.com/redis/go-redis/v9"
	"shared/savebarrier"
)

// MarkTerminal 由 go/db 消费侧在确定性拒写(进死信的围栏 / 归属 / 路由结局，大字段闸等)时调用：
// 只要 (ownerEpoch, saveSeq) 仍是该玩家当前的补投意图，就打终态，补投器之后不再投它。
// 意图原字节、pending 与索引成员都不动：这笔写由死信保全并交人工处置，新版本存盘或两表 ACK 齐全时自然离开终态。
// 返回 true 表示确实打上了；版本不匹配或没有屏障返回 false。
func MarkTerminal(ctx context.Context, rc redis.Cmdable, playerID, ownerEpoch, saveSeq uint64, reason string) (bool, error) {
	if rc == nil || playerID == 0 || ownerEpoch == 0 || saveSeq == 0 {
		return false, errors.New("save outbox 终态标记缺少玩家或严格版本")
	}
	n, err := markTerminalScript.Run(ctx, rc, []string{savebarrier.Key(playerID)}, savebarrier.Version(ownerEpoch, saveSeq), reason).Int64()
	return n == 1, err
}

// NoteRetryDeferred 由 go/db 消费侧在把该玩家的严格写延后进重试队列时调用：补投器据此判定「本 key 已有在途重试」，
// InflightWindow 内只推迟、不再另起一条重试链。只在仍有未完成意图时写；没有屏障或意图已清理时什么都不做。
func NoteRetryDeferred(ctx context.Context, rc redis.Cmdable, playerID uint64, now time.Time) error {
	if rc == nil || playerID == 0 || now.UnixMilli() <= 0 {
		return errors.New("save outbox 在途重试标记缺少玩家或时钟")
	}
	return noteRetryScript.Run(ctx, rc, []string{savebarrier.Key(playerID)}, strconv.FormatInt(now.UnixMilli(), 10)).Err()
}
