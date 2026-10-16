// Package inbox 是玩家持久收件箱的核心:类型注册表、owner_epoch 围栏、seq 分配、分片、
// 存储(入箱 / 拉取 / 确认 / 作废 / 查询)、在线提醒与只在领导者上跑的后台循环
// (docs/design/player-inbox-service.md §3–§10)。
package inbox

// ShardCount 是收件箱三张表的 shard 模数(设计 §4.2)。
//
// 与 placement.RouteRowShard 同为 64,但**常量独立定义、不联动修改**:两张表的模数各自是建表时定下的契约,
// 改它等于改全部存量行的主键。TiDB 默认不执行 CHECK 约束,shard 的一致性靠三样东西保证:
// 本文件唯一的计算函数、读出时断言(checkRowShard)、单测。
const ShardCount = 64

// RowShard 返回 player_id 所在的 shard。**全服务唯一的计算入口**,SQL 里一律传它的结果,不在别处取模。
func RowShard(playerID uint64) uint32 {
	return uint32(playerID % ShardCount)
}
