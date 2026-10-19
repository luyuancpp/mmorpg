#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>

#include "proto/common/base/node.pb.h"
#include "services/gate/session/comp/session_info_comp.h"

namespace gate_scene_route
{
// 本次路由相对会话原先的 scene 节点指向有没有换节点。ApplyRoute 是不碰节点注册表的纯逻辑,自己无从
// 知道,由 CompareSceneNode 得出。用枚举而不是 bool,调用点读得出传的是什么。
enum class SceneNodeChange : uint8_t
{
    kUnchanged,
    kChanged,
};

// ---------------------------------------------------------------------------
// CPP-2 进场转发补发的参数(设计与残余见 cross-zone-scene-travel.md "CPP-2 gate 侧进场转发补发")。
// ---------------------------------------------------------------------------

// 总预算 20s。muduo 断线后立即重连,之后按 0.5/1/2/4/8/16s 翻倍退避(Connector.h),累计重连时刻为
// 0、0.5、1.5、3.5、7.5、15.5s,每次连上后 registration_manager 再等 0.5s 才握手:停机 ≤7.5s 的节点约 8s
// 内就绪,停机 8–16s 的约 16s 就绪(余量约 4s),更久的一律放弃、让玩家重登。
// 同时必须远小于客户端等进场的 60s:玩家要先看到准确原因(3023 + 踢线),而不是通用的"等待进入场景超时"。
inline constexpr std::chrono::milliseconds kSceneEntryRetryBudget{20000};
// 节点找不到时只给 3s 发现预算:etcd watch 推送是毫秒到亚秒级,3s 足以覆盖 gate 比 scene_manager 晚发现
// 节点;而节点被摘除后不会以同一实体回来(同 uuid 重注册是同一次调用里先销毁再创建),"找不到"持续
// 超过 3s 就应放弃。它还把"已投递、之后被回滚的路由在找不到节点时迟到改绑"的窗口从 20s 收窄到 3s。
inline constexpr std::chrono::milliseconds kSceneEntryNodeDiscoveryBudget{3000};
// 首次退避 = 扫描周期(scene_entry_dispatch.h kSweepIntervalSeconds),再短扫描也唤不醒。
inline constexpr std::chrono::milliseconds kSceneEntryInitialBackoff{250};
// 退避封顶 2s:每次尝试只是本地判断(不产生远端副作用),封顶是为了在 20s 内仍有足够的检查点,
// 让链路恢复到补发的延迟不超过 2s;握手就绪还会把欠账提前唤醒,不全靠这里。
inline constexpr std::chrono::milliseconds kSceneEntryMaxBackoff{2000};
// 纵深上限:纯时间点下 not_connected 走满 20s 是 14 次,运行时加上 BindSession 与握手唤醒的额外尝试
// 通常 12–16 次;16 挡住"某条路径反复立即唤醒"的缺陷把尝试刷爆,而不是正常情况下的放弃依据。
inline constexpr uint32_t kSceneEntryMaxAttempts{16};

// 只比较、不改写:会话当前的 scene 节点指向相对 targetNodeEntityId 有没有换节点。
// 比的是 gate 上的节点实体号而不是 node_id:同 node_id 的进程重启后是新实体,那边同样没有该玩家。
//
// 旧指向无效一律算 kChanged,不区分"从未绑定"与"绑定过、但节点已被摘除":scene 节点下线时
// OnNodeRemoveEventHandler / ResolveSessionTargetNode(client_session_handler.cpp)会把指向置为
// entt::null,随后 node_gone 迁频道(沿用原 scene_id)或同 node_id 重启后的改派必须重新进场,
// 若把无效指向当"未变化",这条路由会被当成重复事件吞掉。首登(从未收到过路由)不会因此误转发:
// ApplyRoute 要求 session.sceneId != 0。节点以新实体重新注册、而旧进程仍持有玩家实体时会多转发一次
// enterType=0:scene 侧 PlayerEnterGameNode 对已有实体走 EnterScene 复用路径(player_lifecycle.cpp)——
// session 重绑同号、HandleEnterScene 对"已在目标场景"幂等早退、3.2 交接就地收尾有 epoch 复核、
// 0 不置登录态也不触发登录事件。唯一的状态副作用是 3.1 会顺带摘掉 PlayerSceneChangeInFlightComp,
// 即提前放开一次在途换图的"切换中"闸;静态读下来迟到应答找不到在途组件是静默 no-op,未经运行验证。
inline SceneNodeChange CompareSceneNode(const SessionInfo& session, const entt::entity targetNodeEntityId)
{
    return session.GetNodeEntity(common::base::SceneNodeService) != targetNodeEntityId
               ? SceneNodeChange::kChanged
               : SceneNodeChange::kUnchanged;
}

// 把会话的 scene 节点指向改写为 targetNodeEntityId,并返回相对旧指向有没有换节点(先比较、后覆盖)。
// 生产路径已改走 AttemptPendingSceneEntry(只在转发成功后提交指向);本函数保留供已有用例钉住原语语义,
// 不要把它重新接回转发之前 —— 那样转发失败后同一路由再来时会被判成"节点未变化",同 scene_id 换节点
// 这一支就再也补发不了(SceneRouteEntry.NodeChangeForwardFailureIsNotRecoveredByRedelivery)。
inline SceneNodeChange RebindSceneNode(SessionInfo& session, const entt::entity targetNodeEntityId)
{
    const auto change = CompareSceneNode(session, targetNodeEntityId);
    session.SetNodeEntity(common::base::SceneNodeService, targetNodeEntityId);
    return change;
}

// LOGIN_NONE(0) 在换图时仍表示必须通知 scene,因此用 optional 区分无需入场。
// 发送前置检查失败时不消费路由/登录类型。下面第 1、2 支的判定只读会话自身状态,同事件重投仍可恢复;
// 第 3 支靠调用方传入的 nodeChange:调用方必须在转发成功之后才提交节点指向,否则重试时拿到的是
// kUnchanged,恢复不了。生产调用方 AttemptPendingSceneEntry 正是"比较、转发、成功后提交"。
//
// 要不要向 scene 转发进场,四个条件任一成立即转发:
//   1. pendingEnterGsType 非 0:登录入场,以该登录类型转发;
//   2. scene_id 变了:换图 / 换线,以 LOGIN_NONE(0) 转发;
//   3. scene_id 没变但**节点变了**:同样以 LOGIN_NONE(0) 转发;
//   4. scene_id 与节点都没变,但 scene 此前被告知过一个非 0 的 epoch(session.forwardedOwnerEpoch),而路由的 owner_epoch
//      (调用方已写进 session.ownerEpoch)**大于**它:同样以 LOGIN_NONE(0) 转发。
// 四者都不成立才是重复事件,不转发(幂等)。
//
// 第 4 条(docs/design/location-release-binding.md §3.5,审计 load-handover#0):scene_manager 在 location 缺失后按首次落点
// 铸了新 epoch、又恰好派回同一节点同一频道时,只按场景与节点判会把这条路由当重复事件吞掉 —— 持有节点的实体永远停在旧 epoch,
// 下一次真正写盘被 owner CAS 拒绝、实体按"已废黜"销毁(回档)。最常见的触发是同连接上 BindSession 先到、按旧路由补发了旧 epoch,
// 随后新 epoch 的路由才到。scene 侧对这条转发的处置已有:恰好 +1 → 复用实体并把缓存抬到新值;≥ +2 → 中间有别的持有者,
// 销毁重载(player_exit::IsDeposedOnReentry)。只认"更大":epoch 只增不减,迟到的旧路由不值得再转发;0(旧版 scene_manager)
// 永不触发。scene 从没被告知过 epoch(还没转发过,或上次转发带的是 0)时不触发:那时 scene 会在下一次转发里拿到会话当前的值,
// 不存在"停在旧代"的实体,不为此多发一次 0 型进场。转发成功后记下这次带出的 epoch(ForwardPlayerToScene 盖的就是
// session.ownerEpoch)。
//
// 第 3 条不能省:同一个 scene_id 会出现在不同节点上。scene_manager 的 world rebalance
// (go/scene_manager/internal/logic/world_rebalance.go 的 migrateWorldChannel)迁频道时沿用原 scene_id,
// 只改写 scene->node 映射,随后对旧节点 DestroyScene。规划读到"空频道"与改写映射之间若有玩家被预占
// 进旧节点,旧节点排空时会存盘、写 handoff 标记、销毁本地实体并重新请求 EnterScene,scene_manager 可能
// 挑回已迁到新节点的同一个 scene_id。此时只按 scene_id 判定会把这条路由当成重复事件吞掉:会话指向了
// 新节点,新节点上却没有该玩家的实体,旧节点上的也已销毁,玩家在线但哪里都不存在,只能重登。
// 目标节点上无实体时,enterType=0 的进场请求走异步加载后 EnterScene(scene_handler.cpp PlayerEnterGameNode),
// 0 只表示"不置登录态",与跨节点换图是同一条路径。
//
// 2、3 两条都要求 session.sceneId != 0:会话从未收到过路由时没有"已入场的玩家"可言,仍等登录类型
// 到达后按第 1 条入场。注意 sceneId != 0 只表示"此前收到过路由",不保证已入场:路由先于 BindSession
// 到达时下面也会提交 sceneId 而不转发。此时若再来一条换了节点(或换了 scene_id)的路由,会先以 0 转发
// 一次,登录类型随后由 BindSession 补发;scene 侧对这个顺序安全(pending 进场上下文取最新一份,
// 后到的登录类型仍会置登录态并触发登录事件)。
template <typename ForwardEntry>
bool ApplyRoute(SessionInfo& session, const uint64_t targetSceneId, const SceneNodeChange nodeChange,
                ForwardEntry&& forward)
{
    if (targetSceneId == 0) return false;
    std::optional<uint32_t> enterType;
    if (session.pendingEnterGsType != 0)
        enterType = session.pendingEnterGsType;
    else if (session.sceneId != 0 &&
             (session.sceneId != targetSceneId || nodeChange == SceneNodeChange::kChanged ||
              (session.forwardedOwnerEpoch != 0 && session.ownerEpoch > session.forwardedOwnerEpoch)))
        enterType = 0;
    if (enterType.has_value() && !forward(*enterType)) return false;
    session.sceneId = targetSceneId;
    if (enterType.has_value())
    {
        session.pendingEnterGsType = 0;
        session.forwardedOwnerEpoch = session.ownerEpoch;
    }
    return true;
}

// 客户端 LeaveGame(回到选角界面,TCP 不断)之后清空会话的 scene 路由状态(docs/design/location-release-binding.md §3.5 R5)。
// 调用方必须先向原节点代发 ExitGame(gate_scene_entry::OnClientLeaveGame):清掉指向之后 gate 再也不会替这条会话通知那个节点。
// 清掉之后,同一连接上的下一次进场只能由新的路由事件驱动:BindSession 先到时 lastSceneRoute.sceneId == 0,不按 LeaveGame
// 之前的旧路由补发(旧路由的 epoch 已过时,补发出去会让旧节点按旧代复用 / 载入实体);等新路由到达再一起转发。
// 保留 playerId / loginGeneration / sessionVersion:下一次 BindSession 按既有规则处理(换角取新值、同角色取 max)。
// 欠账的索引(scene_entry_dispatch.cpp 的 tlsSessionsWithPendingEntry)由调用方摘;漏摘也无害,扫描找不到欠账会自己删索引。
inline void ResetSceneRoute(SessionInfo& session)
{
    session.SetNodeEntity(common::base::SceneNodeService, entt::null);
    session.sceneId = 0;
    session.pendingEnterGsType = 0;
    session.pendingSceneEntry.reset();
    session.lastSceneRoute = SceneRouteTarget{};
    session.homeZoneId = 0;
    session.ownerEpoch = 0;
    session.forwardedOwnerEpoch = 0;
}

// ---------------------------------------------------------------------------
// 登录代次盖章(docs/design/scene-enter-session-fence.md §4):只抬高、不丢弃。
// 会话上的 loginGeneration 只供 ForwardPlayerToScene 盖章,判新旧是 scene 入场栅栏的事。两处写入
// (BindSession / RoutePlayer)都必须先于调用方转发:OnLoginTypeBound 会同步按 lastSceneRoute 补发,
// OnRouteDecision 会同步尝试转发,盖章都从会话取值。
// ---------------------------------------------------------------------------

// 新写入的身份相对会话原先绑定的角色有没有换人。用枚举而不是 bool,理由同 SceneNodeChange。
enum class PlayerChange : uint8_t
{
    kSamePlayer,
    kAnotherPlayer,
};

// 同一角色取 max:迟到的、不带代次(0)的消息都不能把会话已知的最高代次拉低。
// 换了角色取新值,哪怕更小:沿用上一个角色的代次,会给新角色的入场盖上别人的章。
inline uint64_t MergeLoginGeneration(const uint64_t current, const uint64_t incoming, const PlayerChange change)
{
    return change == PlayerChange::kSamePlayer ? (std::max)(current, incoming) : incoming;
}

// BindSession 到达:把会话绑到 playerId,写入会话版本号与本次登录链的代次;enterGsType 非 0 时记为待入场的
// 登录类型(为 0 时不动,与改动前一致)。
// 是否换了角色必须用**写之前**的 playerId 判定。会话还没绑过角色(kInvalidGuid)按同一角色处理:此前没有
// 别的角色的代次可言,能记下代次的只有不带玩家的路由(带玩家的路由会先把玩家采用进会话)。
// 返回 true = 写入了登录类型,调用方随后应调 OnLoginTypeBound 尝试补发。
inline bool ApplyLoginBinding(SessionInfo& session, const Guid playerId, const uint32_t sessionVersion,
                              const uint64_t loginGeneration, const uint32_t enterGsType)
{
    const auto change = (session.playerId == kInvalidGuid || session.playerId == playerId)
                            ? PlayerChange::kSamePlayer
                            : PlayerChange::kAnotherPlayer;
    session.playerId = playerId;
    session.sessionVersion = sessionVersion;
    session.loginGeneration = MergeLoginGeneration(session.loginGeneration, loginGeneration, change);
    if (enterGsType == 0)
    {
        return false;
    }
    session.pendingEnterGsType = enterGsType;
    return true;
}

// RoutePlayer 到达:写入这次路由决策带来的身份字段,必须先于 OnRouteDecision。
//   - routePlayerId 非 0 且会话还没绑角色(BindSession 可能尚未到达)时采用它;已绑角色时不改写。
//   - homeZoneId / ownerEpoch 属于"本次路由决策",无条件覆盖:两次改派挨近时只有后到事件的 epoch 与
//     Redis 相等;转发失败后的补发、晚到 BindSession 的补发都从会话取,所以不能等转发成功才提交。
//   - 代次只在路由属于会话当前角色(或路由没带玩家)时取 max;属于别的角色时不动 —— 这条路由随后被
//     AttemptPendingSceneEntry 的玩家围栏撤销,它的代次与当前角色无关。
// 不据代次丢弃路由:scene 自发的换图路由带 0,由会话上的值盖章。
inline void ApplyRouteIdentity(SessionInfo& session, const Guid routePlayerId, const uint32_t homeZoneId,
                               const uint64_t ownerEpoch, const uint64_t loginGeneration)
{
    if (routePlayerId != 0 && session.playerId == kInvalidGuid)
    {
        session.playerId = routePlayerId;
    }
    session.homeZoneId = homeZoneId;
    session.ownerEpoch = ownerEpoch;
    if (routePlayerId == 0 || routePlayerId == session.playerId)
    {
        session.loginGeneration =
            MergeLoginGeneration(session.loginGeneration, loginGeneration, PlayerChange::kSamePlayer);
    }
}

// ---------------------------------------------------------------------------
// CPP-2 欠账的纯逻辑。全部不碰节点注册表与网络:解析器、转发器由调用方注入,供单测直接调用。
// 线程模型:只在 gate 主 EventLoop 线程调用(见 pending_scene_entry_comp.h 的 I2)。
// ---------------------------------------------------------------------------

// 本文件里的 std::min 一律写成 (std::min):gate.vcxproj 没定义 NOMINMAX,而 muduo 的 Windows 适配头
// (CrossPlatformAdapterFunction.h)会带进 <Windows.h> 的 min/max 宏,裸写 std::min(a, b) 会被宏展开。

// 第 attemptsSoFar 次失败之后到下一次尝试的间隔:0 或 1 → 250ms,2 → 500ms,3 → 1s,≥4 → 2s。
// 循环翻倍、遇顶即停,attemptsSoFar 再大也不会溢出或空转。
inline std::chrono::milliseconds SceneEntryBackoff(const uint32_t attemptsSoFar)
{
    auto backoff = kSceneEntryInitialBackoff;
    for (uint32_t i = 1; i < attemptsSoFar && backoff < kSceneEntryMaxBackoff; ++i)
    {
        backoff *= 2;
    }
    return (std::min)(backoff, kSceneEntryMaxBackoff);
}

// 只有"本地确定没发出去、且可能随时间自愈"的失败才值得重试(I1)。kInvalidRoute 等再久也不会变对;
// kSent 不是失败。
inline bool IsRetryableForwardResult(const SceneForwardResult result)
{
    switch (result)
    {
    case SceneForwardResult::kNodeNotFound:
    case SceneForwardResult::kNoRpcClient:
    case SceneForwardResult::kNotConnected:
    case SceneForwardResult::kHandshakePending:
        return true;
    default:
        return false;
    }
}

// scene 链路是否可交付进场。返回 nullopt = 就绪;否则返回未就绪的原因。
// "TCP 连上"不等于"可交付":gate 每次连上 scene,registration_manager 要再等 0.5s 才发 NodeHandshake,
// scene 收到握手才为本 gate 挂 RpcSession;在此之前发出的进场 scene 会照常载入玩家,但 NotifyEnterScene
// 等下行推送找不到 RpcSession 会被丢掉(player_message_utils.cpp)。所以要求"在**当前**这条连接上握过手"。
// 按连接对象的地址比对:重连后旧章自然失效(调用方传的是 weak_ptr::lock() 的结果,旧连接已析构时为空,
// 不会与复用了同一地址的新连接混淆),不需要挂断线钩子。
// 参数用 const void* 而不是 TcpConnection*:判定只需要身份,单测可以用局部变量的地址充当连接。
inline std::optional<SceneForwardResult> ClassifySceneLink(const bool rpcClientConnected,
                                                           const void* currentConn,
                                                           const bool currentConnConnected,
                                                           const void* handshakenConn)
{
    if (!rpcClientConnected || currentConn == nullptr || !currentConnConnected)
    {
        return SceneForwardResult::kNotConnected;
    }
    if (handshakenConn == nullptr || handshakenConn != currentConn)
    {
        return SceneForwardResult::kHandshakePending;
    }
    return std::nullopt;
}

enum class SceneEntryRetryVerdict : uint8_t
{
    kRetry,
    kGiveUp,
};

// 记下一次失败并决定重试还是放弃。重试时把 nextAttemptAt 推到退避之后,但不越过截止:
// 最后一次尝试恰好落在截止时刻,超时判定不会因为扫描网格而多拖一个退避周期。
// 节点找不到时截止收紧到 routedAt + 发现预算;"先连不上、后来节点消失"在超过发现预算的那次尝试上立即放弃。
inline SceneEntryRetryVerdict RecordSceneEntryFailure(PendingSceneEntry& entry, const SceneForwardResult failure,
                                                      const SceneEntryClock::time_point now)
{
    if (entry.firstFailure == SceneForwardResult::kSent)
    {
        entry.firstFailure = failure;
    }
    entry.lastFailure = failure;

    if (!IsRetryableForwardResult(failure))
    {
        return SceneEntryRetryVerdict::kGiveUp;
    }

    auto limit = entry.deadline;
    if (failure == SceneForwardResult::kNodeNotFound)
    {
        limit = (std::min)(limit, entry.routedAt + kSceneEntryNodeDiscoveryBudget);
    }
    if (now >= limit || entry.attempts >= kSceneEntryMaxAttempts)
    {
        return SceneEntryRetryVerdict::kGiveUp;
    }

    entry.nextAttemptAt = (std::min)(now + SceneEntryBackoff(entry.attempts), limit);
    return SceneEntryRetryVerdict::kRetry;
}

// 一条路由决策的新欠账:立即到期,总预算从路由到达时刻起算。
inline PendingSceneEntry BeginPendingSceneEntry(const SceneRouteTarget& target, const SceneEntryClock::time_point now)
{
    PendingSceneEntry entry;
    entry.target = target;
    entry.attempts = 0;
    entry.routedAt = now;
    entry.nextAttemptAt = now;
    entry.deadline = now + kSceneEntryRetryBudget;
    return entry;
}

// BindSession 晚到(登录类型在路由之后才到)时,是否要按最近一次路由建一笔欠账。
// 以前按"已提交的节点指向"补发:指向被节点摘除置无效、或因同 uuid 重注册而悬空时,登录类型会永远挂着。
// 现在一律按最近一次路由的 node_id 重新解析,有上限、超限踢线。
// 返回 nullopt 的几种情形:
//   - 已有欠账:调用方应直接对现有欠账立即尝试一次(ApplyRoute 会优先带上登录类型),不另建第二笔;
//   - 没有待入场的登录类型;
//   - 还没收到过路由(sceneId == 0):等路由自己到来;
//   - 最近一次路由属于另一名角色:等这名角色自己的路由。
inline std::optional<PendingSceneEntry> EntryForLateLoginBinding(const SessionInfo& session,
                                                                 const SceneEntryClock::time_point now)
{
    if (session.pendingSceneEntry.has_value() || session.pendingEnterGsType == 0)
    {
        return std::nullopt;
    }
    const auto& route = session.lastSceneRoute;
    if (route.sceneId == 0)
    {
        return std::nullopt;
    }
    if (route.playerId != 0 && session.playerId != kInvalidGuid && route.playerId != session.playerId)
    {
        return std::nullopt;
    }
    return BeginPendingSceneEntry(route, now);
}

enum class SceneEntryAttempt : uint8_t
{
    kApplied,        // 已转发并提交,或本来就不需要转发;欠账已清(没有欠账时也返回它)
    kRetryLater,     // 本地没发出去,仍在预算内;nextAttemptAt 已更新
    kGiveUp,         // 超预算、超次数或不可重试;欠账保留,由调用方记日志后清理并踢线
    kPlayerMismatch, // 欠账属于另一名角色;什么都没动,由调用方撤销欠账
};

// 对会话上的欠账做一次尝试:比较、转发、成功后才提交(I5)。
//   resolve(nodeId) -> std::optional<entt::entity>:按业务 node_id 解析 gate 上的节点实体号(I3,每次重解析);
//   forward(enterType, nodeEntityId, sceneId) -> SceneForwardResult。
// 失败窗口里会话的 sceneId 与节点指向都不动,所以重试时 CompareSceneNode 仍对着旧节点比,同 scene_id
// 换节点照样判成 kChanged 并补发 0,不需要任何记忆位;放弃时断线 ExitGame 也落到旧节点,而不是一个
// 从来没有过这名玩家的目标节点。
template <typename ResolveNodeEntity, typename Forward>
SceneEntryAttempt AttemptPendingSceneEntry(SessionInfo& session, const SceneEntryClock::time_point now,
                                           ResolveNodeEntity&& resolve, Forward&& forward)
{
    if (!session.pendingSceneEntry.has_value())
    {
        return SceneEntryAttempt::kApplied;
    }
    auto& entry = *session.pendingSceneEntry;
    // 拷一份:下面的 forward 与提交都不应受欠账对象生命周期的影响。
    const SceneRouteTarget target = entry.target;

    // 玩家围栏:同一条连接上换了角色时,绝不以错的角色补发。事件没带玩家(0)或会话尚未绑定玩家时不设防,
    // 与 LeaseExpired 的同款防御一致。
    if (target.playerId != 0 && session.playerId != kInvalidGuid && target.playerId != session.playerId)
    {
        return SceneEntryAttempt::kPlayerMismatch;
    }

    ++entry.attempts;

    // scene_id 为 0 的路由以前被 ApplyRoute 静默忽略;现在 fail-closed:直接放弃并踢线,不解析、不转发。
    if (target.sceneId == 0)
    {
        RecordSceneEntryFailure(entry, SceneForwardResult::kInvalidRoute, now);
        return SceneEntryAttempt::kGiveUp;
    }

    const std::optional<entt::entity> nodeEntity = resolve(target.nodeId);
    if (!nodeEntity.has_value())
    {
        return RecordSceneEntryFailure(entry, SceneForwardResult::kNodeNotFound, now) == SceneEntryRetryVerdict::kRetry
                   ? SceneEntryAttempt::kRetryLater
                   : SceneEntryAttempt::kGiveUp;
    }

    const auto change = CompareSceneNode(session, *nodeEntity);
    auto result = SceneForwardResult::kSent;
    const bool applied = ApplyRoute(session, target.sceneId, change, [&](const uint32_t enterType)
    {
        result = forward(enterType, *nodeEntity, target.sceneId);
        return result == SceneForwardResult::kSent;
    });
    if (applied)
    {
        session.SetNodeEntity(common::base::SceneNodeService, *nodeEntity);
        session.pendingSceneEntry.reset();
        return SceneEntryAttempt::kApplied;
    }
    return RecordSceneEntryFailure(entry, result, now) == SceneEntryRetryVerdict::kRetry
               ? SceneEntryAttempt::kRetryLater
               : SceneEntryAttempt::kGiveUp;
}
}
