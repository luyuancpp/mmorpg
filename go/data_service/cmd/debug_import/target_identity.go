package main

import (
	"context"
	"fmt"
	"strings"

	"data_service/cmd/debugutil"
)

// ── 写前闸门二:写目标不能是导出来源(2026-10-22,r2-ds#1)──────────────────────────
//
// 地址闸门(CheckLocalWriteTarget)只认回环地址,挡不住 port-forward 到 127.0.0.1 的线上实例;这道闸门按实例身份
// 再挡一次。旧实现把取不到的身份当成「不同」直接跳过,而 TiDB 取不到 @@server_uuid,闸门对 TiDB 等于没有:
// 线上 TiDB 经 port-forward 混进配置时,导入会把线上两张玩家行替换成导出时的旧状态并抬高行内围栏,
// 之后线上 scene 的存盘全被判 stale_owner。
//
// 现在每个写目标都必须**证明**自己不是导出来源,证明不了就拒绝(fail-closed)。三种证明,按顺序:
//  1. 身份比对:目标身份取得到,导出包里同类来源实例的身份也都取得到,且没有一个相等;
//  2. 本地调试环境标记:目标库里有 debugutil.LocalTargetMarkerTable 那一行(只对 MySQL / TiDB 目标);
//  3. 操作者显式加 -allow-unverified-target-identity:放行并逐个打 WARNING。
//
// 身份相等一律拒绝,标记与显式放行都不能越过:那是唯一能被正面证明的「写回导出来源」。

// instanceKind 是实例身份的口径。MySQL 的 @@server_uuid 与 Redis 的 run_id 不是同一种值,只在同口径内比较,
// 「同口径来源身份未知」也只影响同口径的目标。
type instanceKind uint8

const (
	instanceMySQL instanceKind = iota
	instanceRedis
)

// exportedInstance 是导出包记录的一个来源实例;id 为空 = 导出时没取到身份(TiDB、旧版导出包、导出时连接失败)。
type exportedInstance struct {
	kind instanceKind
	what string
	id   string
}

// importTarget 是导入要写的一个实例。id 为空 = 取不到身份;localMarker 只对 MySQL / TiDB 目标取值。
type importTarget struct {
	kind        instanceKind
	what        string
	id          string
	localMarker bool
}

// bundleExportInstances 列出导出包实际读过的来源实例。地址与身份都空 = 导出时没读这台实例(例如 -skip-global、
// 落点畸形没读落点库),它不可能是写目标的来源,不参与判定。
func bundleExportInstances(src debugutil.BundleSources) []exportedInstance {
	var out []exportedInstance
	add := func(kind instanceKind, what, addr, id string) {
		if strings.TrimSpace(addr) == "" && id == "" {
			return
		}
		out = append(out, exportedInstance{kind: kind, what: what, id: id})
	}
	add(instanceRedis, "核心 Redis", src.CoreRedis, src.CoreRedisID)
	add(instanceRedis, "映射 Redis", src.MappingRedis, src.MappingRedisID)
	add(instanceMySQL, "落点库", src.ZoneMySQL, src.ZoneMySQLID)
	add(instanceMySQL, "全局库", src.GlobalMySQL, src.GlobalMySQLID)
	return out
}

// mysqlImportTarget 取一个 MySQL / TiDB 写目标的身份与本地标记;q 必须连在导入要写的那个库上。
func mysqlImportTarget(ctx context.Context, what string, q debugutil.Queryer) importTarget {
	return importTarget{
		kind:        instanceMySQL,
		what:        what,
		id:          debugutil.MySQLServerIdentity(ctx, q),
		localMarker: debugutil.HasLocalTargetMarker(ctx, q),
	}
}

// judgeImportTargets 判定全部写目标。返回的 warnings 是凭显式放行通过的目标(调用方逐条打印);
// err 非 nil 时一个都不许写。纯函数,不连任何实例。
func judgeImportTargets(sources []exportedInstance, targets []importTarget, allowUnverified bool) (warnings []string, err error) {
	for _, target := range targets {
		if target.id == "" {
			continue
		}
		for _, source := range sources {
			if source.kind == target.kind && source.id == target.id {
				return nil, fmt.Errorf("%s 与导出来源%s是同一个实例(身份 %s):导入只写隔离环境,检查是不是把线上 port-forward 当成了本地",
					target.what, source.what, target.id)
			}
		}
	}

	var unverified []string
	for _, target := range targets {
		if reason := unverifiedReason(sources, target); reason != "" {
			unverified = append(unverified, target.what+"("+reason+")")
		}
	}
	if len(unverified) == 0 {
		return nil, nil
	}
	if !allowUnverified {
		return nil, fmt.Errorf("以下写目标无法证明不是导出来源,拒绝写入: %s。确认它们都是本地隔离环境后二选一:"+
			"在每个要写的 MySQL / TiDB 库(落点库 zone_<N>_db 与全局库)执行 %s 建本地标记;或加 -allow-unverified-target-identity 显式放行",
			strings.Join(unverified, "; "), debugutil.LocalTargetMarkerSetupSQL)
	}
	for _, item := range unverified {
		warnings = append(warnings, fmt.Sprintf("write target %s cannot be proven distinct from the export source; proceeding only because -allow-unverified-target-identity is set", item))
	}
	return warnings, nil
}

// unverifiedReason 返回目标无法证明「不是导出来源」的原因;能证明时返回空串。
func unverifiedReason(sources []exportedInstance, target importTarget) string {
	if target.localMarker {
		return ""
	}
	if target.id == "" {
		if target.kind == instanceMySQL {
			return "目标实例身份取不到(TiDB 一律取不到),目标库也没有本地调试环境标记"
		}
		return "目标实例身份取不到"
	}
	for _, source := range sources {
		if source.kind == target.kind && source.id == "" {
			return "导出包没记下" + source.what + "的实例身份(来源是 TiDB、旧版导出包或导出时连接失败),无从比对"
		}
	}
	return ""
}
