package logic

import (
	"fmt"
	"slices"
	"strconv"

	"scene_manager/internal/svc"

	"github.com/zeromicro/go-zero/core/logx"
)

// zone 登记集合 scene_manager:zones(2026-10-16,替代领导者 fullSync 里的两次全库 SCAN)。
//
// 是什么:SET,成员是 zone 的十进制。语义是「可能留有 scene_manager 热状态(负载集成员 / 世界频道
// 期望数)的 zone」。它只是**发现索引**,不是存活判据,也不参与路由:多登记一个 zone 只多一次空查;
// 漏登记一个 zone 只会让那个 zone 的陈旧状态多留一阵(见下面的缺口)。
//
// 为什么需要:领导者 fullSync 要清扫「etcd 快照里已经没有、但 Redis 负载集里还留着成员」的 zone
// (sweepStaleLoadSetMembers),以及 World 表删掉的地图留下的频道组(CleanupOrphanWorldChannels)。
// 以前靠 SCAN scene_nodes:zone:*:load 与 SCAN world_channels:zone:* 去发现,SCAN 遍历的是整个共享库,
// 开服规模下单次是小时级(容量模型 §0.6 / 全局单点交接 §5.1 推算,未实测),而且跑在 load reporter 的
// goroutine 上,期间 watch、负载刷新、推迟摘除全部停摆。
//
// 写入方(全部「尽力登记」:失败只打 Error,不挡主流程):
//   - 领导者负载刷新(load_refresh.go refreshZoneLoadSets):每次一条 SADD 覆盖本次涉及的全部 zone,
//     先于同一次的负载集写入。每拍刷新与 watch PUT 的单节点刷新都走这里;
//   - DesiredWorldChannelCount 的播种分支、setDesiredWorldChannelCount:先于写期望数 Hash。这两处
//     可能跑在任意副本上 —— CreateScene 兜底会拿请求里的 zone 走 initWorldScenesForZone,所以集合里
//     可能有一个活节点都没有的 zone。无害,只多一次空查;
//   - 每个副本启动时登记自己配置的 ZoneId(registerOwnZoneOnStartup),补上混合版本窗口里
//     「旧领导者在任期间出现、又整体死掉」的 zone。
//
// 集合只加不减,多个写者并发是安全的:不存在负载集那种「跟随者把死节点加回去」的问题。
//
// 唯一删除方:tools/merge_zone -clear-source-hot-state,在源 zone 的热状态全部清完之后最后一步摘除。
//
// 读取方:sweepStaleLoadSetMembers、CleanupOrphanWorldChannels。两者都扫「快照 zone ∪ 登记集合」;
// 读登记集合失败时退回只扫快照 zone,与旧 SCAN 出错即 break 的退化方向一致。
//
// 不变量:尽力登记 + 每拍自愈。登记写失败**不扣负载集成员资格**(成员资格是存活信号,见
// load_refresh.go 文件头);有活节点的 zone 一定在 etcd 快照里,领导者每拍都会重新登记。剩下的缺口只有:
//
//	(a) 升级前就已整体下线、负载集里还留着成员的 zone(旧版本不登记);
//	(b) 从没登记成功、又在领导者缺位时整体死掉的 zone。
//
// 这两类 zone 复活时会重新进快照,下一次 fullSync 照常清扫;一直不复活的,按
// docs/ops/scene-node-role-split.md §6.1 的一次性运维命令补登记。生产代码里**不保留**全库 SCAN,
// 也不设「打开就 SCAN」的开关 —— 忘关就是把小时级 SCAN 原样带回 load reporter 的 goroutine。
//
// 键名刻意不以 scene_nodes:zone: / world_channels:zone: / world_channels:desired:zone: / node:zone:
// 开头,避免被旧版本 scene_manager 的 SCAN 模式和 merge_zone 按源 zone 前缀清理的模式误命中。
// 字面量与 tools/merge_zone/scene_hot_state.go 的 sceneManagerZonesKey 是跨 module 契约,两边测试各自钉死。
const sceneManagerZonesKey = "scene_manager:zones"

// registerSceneManagerZones 把 zones 去重后用一条 SADD 登记进 scene_manager:zones。空参数不发命令。
// zone 0 不特殊处理:有节点注册成 zone 0 时它同样有负载集,与旧 SCAN 的覆盖范围一致。
func registerSceneManagerZones(svcCtx *svc.ServiceContext, zones ...uint32) error {
	if len(zones) == 0 {
		return nil
	}
	unique := unionZones(zones)
	members := make([]any, 0, len(unique))
	for _, z := range unique {
		members = append(members, strconv.FormatUint(uint64(z), 10))
	}
	if _, err := svcCtx.Redis.Sadd(sceneManagerZonesKey, members...); err != nil {
		return fmt.Errorf("sadd %s (%d zones): %w", sceneManagerZonesKey, len(unique), err)
	}
	return nil
}

// listRegisteredZones 读登记集合,按数值升序返回。不是 uint32 十进制的成员打 Error 后跳过:
// 一条脏数据不能挡住整轮清扫。
func listRegisteredZones(svcCtx *svc.ServiceContext) ([]uint32, error) {
	members, err := svcCtx.Redis.Smembers(sceneManagerZonesKey)
	if err != nil {
		return nil, fmt.Errorf("smembers %s: %w", sceneManagerZonesKey, err)
	}
	zones := make([]uint32, 0, len(members))
	for _, m := range members {
		z, perr := strconv.ParseUint(m, 10, 32)
		if perr != nil {
			logx.Errorf("[ZoneRegistry] skipping malformed member %q in %s: %v", m, sceneManagerZonesKey, perr)
			continue
		}
		zones = append(zones, uint32(z))
	}
	return unionZones(zones), nil
}

// unionZones 合并若干 zone 列表,去重后升序返回。顺序确定,日志与测试才可复现。
func unionZones(lists ...[]uint32) []uint32 {
	set := make(map[uint32]struct{})
	for _, list := range lists {
		for _, z := range list {
			set[z] = struct{}{}
		}
	}
	out := make([]uint32, 0, len(set))
	for z := range set {
		out = append(out, z)
	}
	slices.Sort(out)
	return out
}

// registerOwnZoneOnStartup 在本副本启动时登记自己配置的 ZoneId(每个副本都做,不走领导者闸门)。
// 跟随者此后一条 Redis 命令都不发,这是它唯一一次写登记集合。ZoneId 为 0 视为未配置(单测 / 本机
// 默认配置),不登记。失败只打 Error:领导者每拍还会为有活节点的 zone 重新登记。
func registerOwnZoneOnStartup(svcCtx *svc.ServiceContext) {
	zoneID := svcCtx.Config.ZoneId
	if zoneID == 0 {
		return
	}
	if err := registerSceneManagerZones(svcCtx, zoneID); err != nil {
		logx.Errorf("[ZoneRegistry] failed to register own zone %d on startup: %v", zoneID, err)
	}
}
