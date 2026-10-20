package logic

import "fmt"

const (
	locationKeyPrefix        = "player:location:"
	sessionKeyPrefix         = "player:session:"
	sceneLocationKeyPrefix   = "player:"
	LeaseZSetKey             = "player:leases"
	LeaseProcessingZSetKey   = "player:leases:processing"
	leaseClaimTokenHashKey   = "player:leases:claim_tokens"
	leaseClaimPayloadHashKey = "player:leases:claim_payloads"
)

// 会话按 gate 实例建的派生索引与对账控制面(docs/design/player-locator-gate-session-index.md)。
//
// 为什么用 player: 前缀而不是容量文档建议的 gate:{uuid}:sessions:gate:{id}:draining / drained 属于排队族,
// G4a 第 1 批要整体搬到 redis-queue;索引必须和会话一起留在核心闭包里(同一段会话 Lua 内原子维护),
// 用 player: 前缀让拆分工具与清库守卫按族识别。
// 为什么现在不加 hash tag:索引只在会话 Lua 内和会话键一起被访问,而这些脚本本来就因全局 player:leases* 跨槽,
// 加 tag 换不来任何原子性;上 Cluster 时按 slot 分桶改形,见设计文档「Cluster 前瞻」。
const (
	// gateIndexSessionsKeyPrefix + <gate 实例 uuid>:SET,成员为十进制 player_id。
	// 不变量 I1:会话 ONLINE 且 gate_instance_id 非空 ⇔ player_id 是该 gate 索引的成员。
	gateIndexSessionsKeyPrefix = "player:gate_index:sessions:"
	// GateIndexRegistryKey:SET,成员为 gate 实例 uuid。不变量 I2:索引非空 ⇒ uuid 在登记集里。
	GateIndexRegistryKey = "player:gate_index:gates"
	// gateIndexReadyKey:就绪标记(值 v1:<设置时刻 unix 秒>,带 PX 租约)。存在 = 最近一次完整补建开始之后
	// 没有观察到旧写者,且此后每拍都有 leader 在成功的 etcd 读里确认零旧写者并续期。只由 leader 写。
	gateIndexReadyKey         = "player:gate_index:ready"
	gateIndexReadyValuePrefix = "v1:"
	// gateIndexGenKey:代次计数器。leader 在一次成功的 etcd 读里看到旧写者就 INCR;补建结束时
	// 「代次未变才设就绪标记」,跨 leader 守住「补建期间出现过旧写者」这件事。
	gateIndexGenKey = "player:gate_index:gen"
	// gateIndexLastAuditKey:上一次完整补建 / 审计完成的 unix 秒。审计时机以它为准,换主不重置。
	gateIndexLastAuditKey = "player:gate_index:last_audit"
	// gateIndexAuditCooldownKey:审计失败后的冷却键(值 1,带 EX)。存在时审计不到期,换主不重置。
	gateIndexAuditCooldownKey = "player:gate_index:audit_cooldown"
	// reconcilerLeaderKey:会话对账的选主锁(shared/leader,带 TTL)。作用域 = 一个会话 Redis 域,
	// 今天所有 zone 的 locator 共用一个 Redis(D12),所以全服一个 leader。
	reconcilerLeaderKey = "player_locator:session_reconciler:leader"
)

// gateSessionIndexKey 返回 gate 实例的会话索引键;gateUUID 为空时返回空串。
// 空串是会话 Lua 里「这一侧没有索引」的哨兵,脚本以 ~= "" 守卫,保证永不写入名为 "" 的键。
func gateSessionIndexKey(gateUUID string) string {
	if gateUUID == "" {
		return ""
	}
	return gateIndexSessionsKeyPrefix + gateUUID
}

func locationKey(uid int64) string {
	return fmt.Sprintf("%s%d", locationKeyPrefix, uid)
}

func sessionKey(playerID uint64) string {
	return fmt.Sprintf("%s%d", sessionKeyPrefix, playerID)
}

// sceneManagerLocationKey 是 SceneManager 的位置权威键。player_locator 自己的
// player:location:{id} 是历史 RPC 的另一套 key，生产入场链不会写它。
func sceneManagerLocationKey(playerID uint64) string {
	return fmt.Sprintf("%s%d:location", sceneLocationKeyPrefix, playerID)
}
