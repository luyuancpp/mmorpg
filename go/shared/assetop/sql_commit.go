package assetop

import (
	"context"
	"fmt"
	"shared/savebarrier"

	"github.com/prometheus/client_golang/prometheus"
	"github.com/prometheus/client_golang/prometheus/promauto"
	"github.com/redis/go-redis/v9"
	"github.com/zeromicro/go-zero/core/logx"
	"google.golang.org/protobuf/proto"
	assetpb "proto/common/asset"
)

// SQLCommitKey 只缓存 go/db 已提交的资产账本证据，不是玩家资产的第二份权威。
//
// 键族登记在 docs/design/shared-redis-split-by-purpose.md §2.2；生命周期（不设 TTL、合服与回档要删）见
// docs/design/db-write-fencing-mysql.md §6.6。tools/merge_zone 不依赖本模块，在 player_rows.go 里抄了同一格式，
// 改这里要同改那边（那边有单测钉住字面量）。
func SQLCommitKey(playerID uint64) string {
	return fmt.Sprintf("player:{%d}:asset_sql_commit", playerID)
}

// SQLCommitVersion 是证据版本串：两段 20 位定宽十进制（容纳 uint64 全范围），中间一个冒号，共 41 字节。
// 定宽之后逐字节比较的先后就是 (owner_epoch, save_seq) 的字典序，Lua 里不必也不能把它转成数字比较。
func SQLCommitVersion(ownerEpoch, saveSeq uint64) string {
	return savebarrier.Version(ownerEpoch, saveSeq)
}

// publishSQLCommitScript 只升不降地发布证据。KEYS[1] 证据键；ARGV[1] 版本串（SQLCommitVersion）；ARGV[2] 证据内容。
//
// 版本比较逐字节做，不用 Lua 字符串的 < / >：那两个运算走 strcoll，结果随 Redis 进程的 locale 变
// （与 go/db luaDecimalGreaterFn 同一约定）。参数与存量都先校验为定宽格式（41 字节、第 21 字节是冒号、
// 其余是数字），等长定宽串的逐字节比较才等于数值比较。
//
// 返回 {1}：已写入，或同版本同内容（幂等重放）；{2, 已有版本}：已有证据版本更高，不回退；
// {-1}：同版本不同内容，什么都不写。版本串畸形回 error_reply，不改存量。
const publishSQLCommitScript = `
local function is_version(s)
    return type(s) == 'string' and #s == 41 and string.sub(s, 21, 21) == ':'
        and string.match(s, '^%d+:%d+$') ~= nil
end
local function version_gt(a, b)
    for i = 1, #a do
        local x, y = string.byte(a, i), string.byte(b, i)
        if x ~= y then
            return x > y
        end
    end
    return false
end
local new = ARGV[1]
if not is_version(new) then
    return redis.error_reply('asset sql commit version is malformed: ' .. tostring(new))
end
local old = redis.call('HGET', KEYS[1], 'version')
if old then
    if not is_version(old) then
        return redis.error_reply('stored asset sql commit version is malformed: ' .. old)
    end
    if old == new then
        if redis.call('HGET', KEYS[1], 'data') ~= ARGV[2] then
            return {-1}
        end
        return {1}
    end
    if version_gt(old, new) then
        return {2, old}
    end
end
redis.call('HSET', KEYS[1], 'version', new, 'data', ARGV[2])
return {1}
`

// sqlCommitScript 走 EVALSHA，NOSCRIPT 时由 go-redis 自动回落 EVAL。
var sqlCommitScript = redis.NewScript(publishSQLCommitScript)

// 证据发布结果，即 assetop_sql_commit_publish_total 的 result label。
const (
	sqlCommitWritten  = "written"           // 已写入，或同版本同内容的幂等重放
	sqlCommitAhead    = "evidence_ahead"    // 已有证据版本更高，本次不回退
	sqlCommitConflict = "evidence_conflict" // 同版本不同内容，什么都没写
	sqlCommitError    = "error"             // Redis 往返失败、版本串畸形或脚本回了意外的值
)

// sqlCommitPublishTotal 是 assetop_sql_commit_publish_total{result}。
//
// 注册在默认 registry，由写入方 go/db 的 promhttp.Handler() 暴露（同 shared/playersync 的 bumpTotal）；
// 引用本包的其他服务只会多一条恒为 0 的序列。evidence_ahead 在正常运行中不该出现：更新的版本必然先发布过证据，
// 出现通常是回档 / PITR 之后没清证据键（db-write-fencing-mysql.md §9.2），证据指向已被回滚的提交，必须告警。
// 过渡指标：证据发布并入 go/db 的令牌发布脚本之后，由 db_fence_cache_publish_total 的同名 result 取代（同文 §6.2、§10）。
var sqlCommitPublishTotal = promauto.With(prometheus.DefaultRegisterer).NewCounterVec(prometheus.CounterOpts{
	Name: "assetop_sql_commit_publish_total",
	Help: "Asset SQL commit evidence publications by result (written|evidence_ahead|evidence_conflict|error). " +
		"evidence_ahead means newer evidence is already stored (usually a rollback that did not clear the key) and must alert.",
}, []string{"result"})

// PublishSQLCommit 只能在资产所在 SQL 行已提交后调用。失败必须保留原待办重试。
//
// 比较版本防止较旧事务的迟到发布覆盖新证据：已有证据更高时不回退、计数并打 ERROR，但不回错误（写已落库，
// 任务照常确认）；相同版本不同内容是故障，计数并回错误，不能伪装成功。
func PublishSQLCommit(ctx context.Context, client redis.Cmdable, commit *assetpb.PlayerAssetSQLCommit) error {
	if commit == nil || commit.GetPlayerId() == 0 || commit.GetOwnerEpoch() == 0 || commit.GetSaveSeq() == 0 {
		return fmt.Errorf("资产 SQL 提交证据缺少版本")
	}
	data, err := (proto.MarshalOptions{Deterministic: true}).Marshal(commit)
	if err != nil {
		return err
	}
	version := SQLCommitVersion(commit.GetOwnerEpoch(), commit.GetSaveSeq())
	reply, err := sqlCommitScript.Run(ctx, client, []string{SQLCommitKey(commit.GetPlayerId())}, version, data).Slice()
	if err != nil {
		sqlCommitPublishTotal.WithLabelValues(sqlCommitError).Inc()
		return fmt.Errorf("发布资产 SQL 提交证据: %w", err)
	}
	code := int64(0)
	if len(reply) > 0 {
		code, _ = reply[0].(int64)
	}
	switch code {
	case 1:
		sqlCommitPublishTotal.WithLabelValues(sqlCommitWritten).Inc()
		return nil
	case 2:
		stored := ""
		if len(reply) > 1 {
			stored, _ = reply[1].(string)
		}
		sqlCommitPublishTotal.WithLabelValues(sqlCommitAhead).Inc()
		logx.Errorf("[AssetOp] ASSET-SQL-COMMIT-AHEAD stored evidence is newer than this commit; not rolled back "+
			"(clear the evidence key after any rollback) player_id=%d stored_version=%s new_version=%s",
			commit.GetPlayerId(), stored, version)
		return nil
	case -1:
		sqlCommitPublishTotal.WithLabelValues(sqlCommitConflict).Inc()
		logx.Errorf("[AssetOp] ASSET-SQL-COMMIT-CONFLICT same evidence version with different ledger bytes; nothing written player_id=%d version=%s",
			commit.GetPlayerId(), version)
		return fmt.Errorf("资产 SQL 提交证据同版本内容冲突 player_id=%d version=%s", commit.GetPlayerId(), version)
	default:
		sqlCommitPublishTotal.WithLabelValues(sqlCommitError).Inc()
		return fmt.Errorf("发布资产 SQL 提交证据: 脚本返回了意外的值 %v", reply)
	}
}
