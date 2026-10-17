package clientplayerloginlogic

import (
	"context"
	"errors"
	"login/internal/logic/pkg/dataloader"
	login_proto_common "proto/common/base"
	"shared/generated/pb/table"
	"time"

	"github.com/zeromicro/go-zero/core/logx"
)

// newPlayerSeedTimeout 是种子写的 ctx 预算。理由同 accountBlobReadBackTimeout:它**封不住单次调用**
// (login 的 Redis 客户端没开 ContextTimeoutEnabled,单次 socket 读仍可等满 go-redis 默认的 3s),
// 真正限制的是 go-redis 的内部重发(默认 MaxRetries=3)。种子只是优化,不值得在账号锁里多等几轮。
const newPlayerSeedTimeout = time.Second

// probeMintedID 是 CreatePlayer 的 6b' 步:在登记名字(6c)之前,只读探测新铸 id 上是否已有既有状态
// (dataloader.ProbeMintedPlayerState:PlayerAllData 父键、player:{id}:owner_epoch;
// docs/design/new-player-first-enter-no-cold-read.md §10 P7,负责人 2026-10-15 按「最标准」定为采用)。
//
// 返回 nil = 两把键都不在,可以继续;返回 tip = 必须拒绝建角。命中(id 复用)与探测出错一律拒绝(fail-closed):
// 号段 id 不复用(设计 I1),新铸 id 上有状态说明发号器把在役 id 又发了一次,或 id_segment 表被重置而 Redis 没清,
// 把它交给新账号就是串档;探不清时放行,同样可能把一个在役 id 交出去。拒绝发生在任何持久写之前:名字、
// player:zone 都还没登记,只烧掉这一个 id,不需要补偿。玩家重试会铸出下一个 id。
//
// 与种子开关无关,每次建角都探:关着种子时,id 复用同样会让新账号首次入场读到那份旧缓存。
// 探测与 6d' 种子写之间仍有窗口:开种子时,种子脚本的条件判断保留为这一段的写时原子复核(见 seedNewPlayer);
// 关种子(默认)时探测之后、开种子时种子之后,到第 7 步围栏写都没有复核,是已知残余窗口(设计 §13.2 2026-10-16 补注)。
//
// tip 复用 6c / 6d 故障时返回的 kLoginDataSerializeFailed(「服务端此刻建不了角,稍后重试」),理由同
// registerHomeZone:不为一个分支申请新码,Tip 码轴由导表器发号。
//
// 预算(2026-10-16):探测直接用请求 ctx,不另设子预算(原先的 1s mintedIDProbeTimeout 已删)。login 的 Redis
// 客户端没开 ContextTimeoutEnabled,ctx 截止不约束 socket 读写,但 go-redis 取连接(连接池排队)与拨号认 ctx:
// 1s 子预算会让连接池等满超过 1s 的探测以 DeadlineExceeded 失败,而探测 fail-closed,它就成了唯一一个预算短于
// handler、又会拒绝建角的 Redis 步骤(读 blob、围栏写都用请求 ctx)。用请求 ctx 后它对池等待的容忍度与读 blob
// 一致,上界是 handler 截止;6c 之后的写仍由 6c'' 的 abandonAfterDeadline 把关,login.yaml 锁 TTL 的推导不变。
// 只读 EXISTS 在预算内被 go-redis 重发是无害的。
func (l *CreatePlayerLogic) probeMintedID(account string, playerID uint64) *login_proto_common.TipInfoMessage {
	// 发出之前先查请求 ctx:截止之后不再发任何命令(docs/design/client-request-ordering.md §6.1)。
	// 此刻名字、映射都还没登记,直接拒绝、无需补偿;不计探测指标、不打 ERROR —— 探测没有发生,
	// 也不是 Redis 故障。不查的话,go-redis 会以 ctx 错误原地拒掉 pipeline,被误记成一次探测出错。
	if err := l.ctx.Err(); err != nil {
		logx.Infof("[minted-id-probe] request ctx done before probing, nothing sent player_id=%d account=%s: %v",
			playerID, account, err)
		return loginTip(table.LoginError_kLoginDataSerializeFailed)
	}
	result, err := dataloader.ProbeMintedPlayerState(l.ctx, l.svcCtx.RedisClient, playerID)
	if result == dataloader.MintedIDProbeError {
		// 请求在探测途中结束(截止 / 取消):go-redis 在取连接或拨号时以 ctx 错误返回,探测没有结论,也不是
		// Redis 故障。与上面的前置检查同一口径:仍拒绝,不计探测指标、不打 ERROR(metrics.go id_probe_total 注释)。
		// 只认「错误就是请求 ctx 的错误」:不会被 go-redis 重发的 Redis 错误(错误回包、重发用完后的拨号失败等)
		// 即使恰好在截止之后才返回,也照常计 error。读超时、连接池超时这类可重发的错误则不同:截止之后,go-redis
		// 重发前的退避等待会把它换成 ctx 错误,落进本分支、不计数 —— Redis 卡顿只体现在 stage_seconds{stage="id_probe"}。
		if ctxErr := l.ctx.Err(); ctxErr != nil && errors.Is(err, ctxErr) {
			logx.Infof("[minted-id-probe] request ctx done while probing, refusing to create player_id=%d account=%s: %v",
				playerID, account, err)
			return loginTip(table.LoginError_kLoginDataSerializeFailed)
		}
	}
	createPlayerIDProbeTotal.Inc(result.String())
	switch result {
	case dataloader.MintedIDClear:
		return nil
	case dataloader.MintedIDParentExists, dataloader.MintedIDOwnerEpochExists:
		// 与名字登记的 FailedPrecondition 同属 ID 安全事件:运维据此排查发号器 / id_segment 表。
		logx.Errorf("[minted-id-probe] freshly minted player_id=%d already has %s — id reuse? "+
			"refusing to create before reserving the name or registering player:zone (account=%s)",
			playerID, result, account)
	default:
		// MintedIDProbeError,以及将来新增而这里还不认识的结论:探不清,按拒绝处理。
		logx.Errorf("[minted-id-probe] probe failed, refusing to create (fail-closed) player_id=%d account=%s: %v",
			playerID, account, err)
	}
	return loginTip(table.LoginError_kLoginDataSerializeFailed)
}

// seedNewPlayer 是 CreatePlayer 的 6d' 步:写新号的 PlayerAllData 父键种子
// (dataloader.SeedNewPlayerAllData;docs/design/new-player-first-enter-no-cold-read.md)。
//
// 返回 nil = 继续建角;返回 tip = 必须拒绝建角(只有下面两种 id 复用结局)。调用方负责补偿(释放 6c 的名字)。
//
// 种子本身尽力而为:written / idempotent / error 都不改变建角结果。种子是首次入场的缓存优化,不是任何数据的
// 真源;没写成时,首次 EnterGame 照旧冷读数据库,读到空就是真值(号段 id 不复用)。
//
// parent_exists / owner_epoch_exists 对新铸的 id 不该出现:6b' 的只读探测已经确认过这两把键都不在,
// 走到这里还看到,说明它们恰好在探测与种子写之间的窗口里出现 —— 这个 id 上已有既有状态(发号器把在役 id
// 又发了一次,或 id_segment 表被重置而 Redis 没清),与名字登记的 FailedPrecondition 同属 ID 安全事件。
// 负责人 2026-10-15 按「最标准」把它从「只告警」升级为**拒绝建角**(设计 §10 P2;此前的现状是记 ERROR、计数,
// 仍把 id 交给新账号,让首次入场读到那份既有缓存)。拒绝时的残留与第 7 步失败同类:
//   - 6c 登记的名字由调用方按「确定已登记」释放一次;
//   - 6d 登记的 player:zone 映射**撤不掉**:它是 SET NX、无 TTL,login 没有删除入口(data_service 的
//     DeletePlayerData 会连同该 id 的玩家数据一起删,id 复用时删的正是旧持有者的数据,绝不能用)。
//     旧持有者若经现行建角流程创建,映射本来就在(同 zone 时 6d 是幂等空操作,不新增);只有旧持有者没有
//     映射时才会留下一条,与第 7 步失败留下的那条同类,ERROR 日志带 player_id 供核对。
//
// idempotent(父键已是这份种子)单独记一条 INFO:本调用点的 1s ctx 预算掐掉了读超时(3s)之后的重发,
// 所以它只会来自连接层快速失败(EOF / 连接被重置)后的重发,或者这个 id 以前铸出过、那份种子恰好在探测之后
// 才落地(滞留的旧请求)。稳态应接近 0,持续出现要按 id 复用排查。它不拒绝:本次 EVAL 被重发时看到的正是
// 自己写的种子,拒绝就会把正常建角判死。
func (l *CreatePlayerLogic) seedNewPlayer(account string, playerID uint64) *login_proto_common.TipInfoMessage {
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
		logx.Errorf("[new-player-seed] freshly minted player_id=%d already has %s — id reuse? "+
			"seed not written; refusing to create (releasing the reserved name; the player:zone mapping, "+
			"if this attempt created it, cannot be removed and is left behind) (account=%s)",
			playerID, result, account)
		return loginTip(table.LoginError_kLoginDataSerializeFailed)
	default:
		// SeedError = 写入未确认:读超时的 EVAL 可能已经在服务端执行,不能写成「没写成」。
		logx.Errorf("[new-player-seed] seed write not confirmed (it may or may not have landed); "+
			"first EnterGame takes the fast path if it did, otherwise the DB read (player_id=%d account=%s): %v",
			playerID, account, err)
	}
	return nil
}
