package logic

import (
	"fmt"
	"strconv"
	"strings"
	"time"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"proto/scene_manager"
	"scene_manager/internal/svc"
	"shared/ownerepoch"
)

// 删 location 绑定「持有者已释放」(docs/design/location-release-binding.md §3.1 R1、§3.2)。
//
// location 是换手门(CZ-4)的入口条件:它在,跨节点落点必须出示当前 owner_epoch 的 handoff 标记;它不在,下一次进场按首次落点
// 铸造新 epoch、不过门。所以删 location 之前必须证明没有任何节点还持有该玩家在当前代 E 下未落盘的状态,否则新节点会从旧快照起步,
// 持有节点之后的存盘被 owner CAS 拒绝、实体被当作已废黜销毁 —— 回档到上一次成功存盘(审计 §2.1)。

// leaveReleaseVerdict 是 LeaveScene 在离场身份判为可删(legacy / deleted)之后的释放判定。名字同时是
// scene_manager_leave_holder_release_total 的 verdict 标签。底层类型 uint8 数的是结论种类数(现 8 种)。
type leaveReleaseVerdict uint8

const (
	// leaveReleaseDisabled:整项检查关闭(LeaveSceneHolderReleaseWaitSeconds < 0,或 dev 旁路),按旧行为删。
	leaveReleaseDisabled leaveReleaseVerdict = iota
	// leaveReleaseLegacyEpochZero:owner_epoch 键与 location 记的 epoch 都为 0(存量玩家,从未铸造)。这类玩家的存盘本来
	// 不带 CAS,epoch 守卫保护不到,删 location 不会把持有节点之后的存盘变成"被拒后销毁",按旧行为删。
	leaveReleaseLegacyEpochZero
	// leaveReleaseNoHolder:location 的 node_id 为空(跨 zone 传送的等待落点),此刻没有任何节点持有该玩家。
	leaveReleaseNoHolder
	// leaveReleaseOwnerDead:持有节点确认已死且再入屏障已过,或所在 zone 已无任何节点(与 EnterScene 死节点接管同一判据)。
	leaveReleaseOwnerDead
	// leaveReleaseReleased:handoff 标记的 epoch 等于当前代 E —— 持有者已把 E 代状态落盘并不再写(A1′ / 交接 / 改派 / 凭证)。
	// 删除 Lua 里原子复核(R2)。
	leaveReleaseReleased
	// leaveReleaseForced:等满上限仍拿不到证据,按旧行为删除(有记录的 fail-open,§3.6)。应恒 0。
	leaveReleaseForced
	// leaveReleasePending:没有证据、未超限、离场身份可证明 location 属于离场会话(deleted):保留 location,请持有节点释放。
	leaveReleasePending
	// leaveReleasePendingUnproven:同上,但离场身份是 legacy(任一侧无身份,混版窗口):location 可能属于一次更新的登录,
	// 退出它等于踢掉在线玩家,所以只等证据、不发释放(R3)。
	leaveReleasePendingUnproven

	leaveReleaseVerdictCount
)

var leaveReleaseVerdictNames = [...]string{
	leaveReleaseDisabled:        "disabled",
	leaveReleaseLegacyEpochZero: "legacy_epoch0",
	leaveReleaseNoHolder:        "no_holder",
	leaveReleaseOwnerDead:       "owner_dead",
	leaveReleaseReleased:        "released",
	leaveReleaseForced:          "forced",
	leaveReleasePending:         "pending",
	leaveReleasePendingUnproven: "pending_unproven",
}

// 名表长度与 leaveReleaseVerdictCount 不一致时编译失败(漏加 / 多加一行)。
var _ [leaveReleaseVerdictCount]struct{} = [len(leaveReleaseVerdictNames)]struct{}{}

func (v leaveReleaseVerdict) String() string {
	if v < leaveReleaseVerdictCount {
		return leaveReleaseVerdictNames[v]
	}
	return "unknown"
}

// deletes:按这个结论删 location。released 走释放模式(Lua 复核标记),其余删除类走原文 CAS 模式。
func (v leaveReleaseVerdict) deletes() bool {
	switch v {
	case leaveReleaseDisabled, leaveReleaseLegacyEpochZero, leaveReleaseNoHolder, leaveReleaseOwnerDead,
		leaveReleaseReleased, leaveReleaseForced:
		return true
	default:
		return false
	}
}

// waits:没拿到证据、需要先看等待计时的结论。
func (v leaveReleaseVerdict) waits() bool {
	return v == leaveReleasePending || v == leaveReleasePendingUnproven
}

// holderReleaseFacts 是 decideHolderRelease 的输入,全部由调用方采好。默认值取"等"的一侧:漏填只会多等,不会误删。
type holderReleaseFacts struct {
	enabled        bool   // 整项检查开着(EffectiveLeaveSceneHolderReleaseWait 的 enabled)
	epoch          uint64 // 当前代 E:owner_epoch 键,键为 0 时取 location 记的 epoch
	noHolder       bool   // location 的 node_id 为空
	ownerDead      bool   // 持有节点已死 / zone 已无节点
	markerReleased bool   // handoff 标记的 epoch == E
	waitExceeded   bool   // 等待计时已到上限
	identityProven bool   // 离场身份判定为 deleted(location 的登录身份 ≤ 离场会话)
}

// decideHolderRelease 是 docs/design/location-release-binding.md §3.2 判定表,按顺序判,零副作用。
func decideHolderRelease(f holderReleaseFacts) leaveReleaseVerdict {
	switch {
	case !f.enabled:
		return leaveReleaseDisabled
	case f.epoch == 0:
		return leaveReleaseLegacyEpochZero
	case f.noHolder:
		return leaveReleaseNoHolder
	case f.ownerDead:
		return leaveReleaseOwnerDead
	case f.markerReleased:
		return leaveReleaseReleased
	case f.waitExceeded:
		return leaveReleaseForced
	case f.identityProven:
		return leaveReleasePending
	default:
		return leaveReleasePendingUnproven
	}
}

// leaveReleaseEpoch 是判定用的当前代 E:owner_epoch 键非 0 时用它;键为 0(缺键 / 被单独删)时取 location 记的值 ——
// 与 EnterScene「持有节点缓存为 0 只从两处同时为 0 推出」同一口径(enterscenelogic.go 同节点补种段)。
func leaveReleaseEpoch(observedEpoch uint64, loc *scene_manager.PlayerLocation) uint64 {
	if observedEpoch != 0 {
		return observedEpoch
	}
	return loc.GetOwnerEpoch()
}

// markerReleasesEpoch:handoff 标记原文是否证明 epoch 这一代已释放。没有标记 / 写坏都不算(fail-closed)。
func markerReleasesEpoch(markerRaw string, epoch uint64) bool {
	marker, err := ownerepoch.ParseHandoff(markerRaw)
	return err == nil && epoch != 0 && marker.Epoch == epoch
}

// errHolderReleasePending 是 LeaveScene 拿不到释放证据时回给调用方的错误。用 FailedPrecondition:它不是故障,
// go-zero 的熔断不计它;player_locator 按这个码识别"等持有者释放",打 INFO 并按清理通道重试(leasemonitor.go)。
// 消息前缀是固定串,便于日志检索;玩家号等只进日志不进 code。
const holderReleasePendingMessage = "holder_release_pending"

func newHolderReleasePendingError(playerID, sceneID uint64, nodeID string, epoch uint64, verdict leaveReleaseVerdict,
	waited time.Duration) error {
	return status.Error(codes.FailedPrecondition, fmt.Sprintf("%s: player=%d scene=%d node=%s owner_epoch=%d verdict=%s waited=%s",
		holderReleasePendingMessage, playerID, sceneID, nodeID, epoch, verdict, waited.Truncate(time.Second)))
}

// ── 等待计时 ─────────────────────────────────────────────────────────────────
//
// player:{id}:leave_release_wait = "{E}:{node}:{sid}:{gen}:{first_ms}",EX = 上限 + leaveReleaseWaitKeyMargin。
// 第一次拿不到证据时写入;location 的 (E, 节点, 登录身份) 变了就当作一次新的等待重新计时。删 location 的 Lua 顺手删掉它。
// 读写失败一律按"刚开始等"处理:宁可多等一轮,也不因为计时读不出来而提前 forced。

const leaveReleaseWaitKeyMargin = 10 * time.Minute

func leaveReleaseWaitKey(playerID uint64) string {
	return fmt.Sprintf("player:%d:leave_release_wait", playerID)
}

// leaveReleaseWaitScope 是等待计时所属的那一份 location。
func leaveReleaseWaitScope(epoch uint64, loc *scene_manager.PlayerLocation) string {
	return fmt.Sprintf("%d:%s:%d:%d", epoch, loc.GetNodeId(), loc.GetSessionId(), loc.GetLoginGeneration())
}

// parseLeaveReleaseWait 解析等待计时的值,返回 (scope, 首次拒绝的毫秒时间, ok)。
func parseLeaveReleaseWait(raw string) (string, int64, bool) {
	idx := strings.LastIndexByte(raw, ':')
	if idx <= 0 || idx == len(raw)-1 {
		return "", 0, false
	}
	firstMs, err := strconv.ParseInt(raw[idx+1:], 10, 64)
	if err != nil || firstMs <= 0 {
		return "", 0, false
	}
	return raw[:idx], firstMs, true
}

// noteLeaveReleaseWait 返回这份 location 已经等了多久(第一次见到时登记并返回 0)。now 由调用方传入,本函数不读时钟。
func noteLeaveReleaseWait(svcCtx *svc.ServiceContext, playerID uint64, scope string, now time.Time,
	budget time.Duration) time.Duration {
	key := leaveReleaseWaitKey(playerID)
	if raw, err := svcCtx.Redis.Get(key); err == nil {
		if storedScope, firstMs, ok := parseLeaveReleaseWait(raw); ok && storedScope == scope {
			if waited := now.Sub(time.UnixMilli(firstMs)); waited > 0 {
				return waited
			}
			return 0
		}
	}
	ttl := budget + leaveReleaseWaitKeyMargin
	value := scope + ":" + strconv.FormatInt(now.UnixMilli(), 10)
	_ = svcCtx.Redis.Setex(key, value, int(ttl/time.Second))
	return 0
}

// ── 删除 ─────────────────────────────────────────────────────────────────────

// location 删除的两种模式,即删除 Lua 的 ARGV[2]。
const (
	// leaveDeleteModeAny:只比 location 原文(旧语义)。用于例外与"已死 / 没有持有者 / forced"。
	leaveDeleteModeAny = "any"
	// leaveDeleteModeReleased:另外原子复核 owner_epoch 仍为 E(缺键视作未变)且 handoff 标记 epoch == E(R2)。
	leaveDeleteModeReleased = "released"
)

// leaveOutcomeNotReleased:释放模式复核时证据已不在(标记被撤回 / 换代 / owner_epoch 变了),一个字节都没动。
const leaveOutcomeNotReleased = "not_released"
