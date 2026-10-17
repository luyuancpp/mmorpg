// Package data 是 rank 的 Redis 数据层:键、复合分数编码、写入 / 读取两段 Lua(设计 docs/design/leaderboard-system.md §3)。
package data

// RootPrefix 是 rank 全部键的唯一根前缀(设计 §3.2)。
//
// 不能以 guild_rank 开头:go/guild 的 RebuildRanks 会 SCAN guild_rank:zone:* 再 DEL / RENAME,任何以 guild_rank 开头的键都可能在
// 公会启动重建时被删;也避开 match:*(match 已有 match:{mq}:rank:*)。键只由下面的构造函数产生,禁止在别处拼字符串
// (keys_test.go 的 T9 按 AST 枚举本文件的全部构造函数做前缀断言)。
const RootPrefix = "lb:"

// ratingHashTag 把"同一张榜的 ZSET 与版本表"钉在同一个 Redis Cluster slot:写入 Lua 要同时读写两者(契约 §4:跨 key 原子才收 hash tag)。
const ratingHashTag = "{rating}"

// RatingScoresKey 是 match 评分全服榜的 ZSET:member = player_id 十进制串,score = 复合分(score.go)。无 TTL。
func RatingScoresKey() string {
	return RootPrefix + ratingHashTag + ":z"
}

// RatingVersionsKey 是 match 评分全服榜的版本表 HASH:field = player_id,value = "<rating_gen>:<games>"(§3.2)。无 TTL。
// 版本单独放 HASH:score 已经用满 53 位,放不下版本。它同时就是消费幂等的"标记"(契约 §6:TTL ≥ 保留期,这里无 TTL)。
func RatingVersionsKey() string {
	return RootPrefix + ratingHashTag + ":v"
}
