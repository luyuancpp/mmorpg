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
// parent_exists / owner_epoch_exists 对新铸的 id 不该出现。出现即说明这个 id 上已有既有状态(发号器把在役 id
// 又发了一次,或 id_segment 表被重置而 Redis 没清),与名字登记的 FailedPrecondition 同属 ID 安全事件。
// 现状**只告警**:记 ERROR 并计数,不在这里拒绝建角。是否升级为拒绝待负责人拍板(设计 §10 P2),取舍的事实是:
//   - 旧持有者若经现行建角流程创建,它的 player:zone 映射早已存在:同 zone 时 6d 是幂等空操作,此时拒绝
//     不会新增映射;不同 zone 在 6d 就已被拒,到不了这里。只有旧持有者没有映射时,拒绝才会留下一条,
//     与第 7 步失败留下的那条同类,不是这里特有的代价;
//   - 只告警的代价:已确知这个 id 上有既有状态,仍把它交给新账号,首次入场读到那份缓存(与关着种子时
//     相同,但此刻本可拦下);
//   - 若改为拒绝,要像 6d 失败分支一样先释放 6c 登记的名字。
//
// idempotent(父键已是这份种子)单独记一条 INFO:本调用点的 1s ctx 预算掐掉了读超时(3s)之后的重发,
// 所以它只会来自连接层快速失败(EOF / 连接被重置)后的重发,或者这个 id 以前铸出过、那份种子还没过期。
// 稳态应接近 0,持续出现要按 id 复用排查。
func (l *CreatePlayerLogic) seedNewPlayer(account string, playerID uint64) {
	ctx, cancel := context.WithTimeout(l.ctx, newPlayerSeedTimeout)
	defer cancel()
	result, err := dataloader.SeedNewPlayerAllData(ctx, l.svcCtx.RedisClient, playerID)
	createPlayerSeedTotal.Inc(result.String())
	switch result {
	case dataloader.SeedWritten:
		// 正常结局,只计数。
	case dataloader.SeedIdempotent:
		logx.Infof("[new-player-seed] parent key already holds this exact seed (EVAL re-sent after a transport error, "+
			"or this id was minted before and never entered) player_id=%d account=%s", playerID, account)
	case dataloader.SeedParentExists, dataloader.SeedOwnerEpochExists:
		// 这里首次入场**不一定**冷读:父键已在时走快路径,读到的是那份已有缓存;只有 owner_epoch 在、
		// 父键不在时才冷读(设计 §6 第 1 行)。
		logx.Errorf("[new-player-seed] freshly minted player_id=%d already has %s — id reuse? "+
			"seed not written; first EnterGame uses the existing cache, or the DB read if there is none (account=%s)",
			playerID, result, account)
	default:
		// SeedError = 写入未确认:读超时的 EVAL 可能已经在服务端执行,不能写成「没写成」。
		logx.Errorf("[new-player-seed] seed write not confirmed (it may or may not have landed); "+
			"first EnterGame takes the fast path if it did, otherwise the DB read (player_id=%d account=%s): %v",
			playerID, account, err)
	}
}
