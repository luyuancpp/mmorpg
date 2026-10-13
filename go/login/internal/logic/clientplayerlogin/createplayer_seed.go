package clientplayerloginlogic

import (
	"context"
	"login/internal/logic/pkg/dataloader"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
)

// newPlayerSeedTimeout 是种子写的 ctx 预算。理由同 accountBlobReadBackTimeout:它**封不住单次调用**
// (login 的 Redis 客户端没开 ContextTimeoutEnabled,单次 socket 读仍可等满 go-redis 默认的 3s),
// 真正限制的是 go-redis 的内部重发(默认 MaxRetries=3)。种子只是优化,不值得在账号锁里多等几轮。
const newPlayerSeedTimeout = time.Second

// seedNewPlayer 是 CreatePlayer 的 6d' 步:写新号的 PlayerAllData 父键种子
// (dataloader.SeedNewPlayerAllData;docs/design/new-player-first-enter-no-cold-read.md)。
//
// 尽力而为,**不返回错误、不改变建角结果**:种子是首次入场的缓存优化,不是任何数据的真源。没写成时,
// 首次 EnterGame 照旧冷读数据库,读到空就是真值(号段 id 不复用)。
//
// parent_exists / owner_epoch_exists 对新铸的 id 不该出现。出现即说明这个 id 以前被用过(发号器把在役 id
// 又发了一次,或 id_segment 表被重置而 Redis 没清),与名字登记的 FailedPrecondition 同属 ID 安全事件:
// 记 ERROR 并计数,但不在这里拒绝建角 —— 走到这一步 player:zone 已经登记,此时拒绝会留下永久的幽灵映射
// (见 6d 的注释)。是否升级为拒绝待负责人拍板(设计 §10 P2)。
func (l *CreatePlayerLogic) seedNewPlayer(account string, playerID uint64) {
	ctx, cancel := context.WithTimeout(l.ctx, newPlayerSeedTimeout)
	defer cancel()
	result, err := dataloader.SeedNewPlayerAllData(ctx, l.svcCtx.RedisClient, playerID)
	createPlayerSeedTotal.Inc(result.String())
	switch result {
	case dataloader.SeedWritten, dataloader.SeedIdempotent:
		// 正常结局,只计数。
	case dataloader.SeedParentExists, dataloader.SeedOwnerEpochExists:
		// 这里首次入场**不一定**冷读:父键已在时走快路径,读到的是那份已有缓存;只有 owner_epoch 在、
		// 父键不在时才冷读(设计 §6 第 1 行)。
		logx.Errorf("[new-player-seed] freshly minted player_id=%d already has %s — id reuse? "+
			"seed not written; first EnterGame uses the existing cache, or the DB read if there is none (account=%s)",
			playerID, result, account)
	default:
		logx.Errorf("[new-player-seed] seed not written, first EnterGame falls back to the DB read "+
			"(player_id=%d account=%s): %v", playerID, account, err)
	}
}
