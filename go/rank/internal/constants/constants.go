// Package constants 是 rank 写进 TipInfoMessage.id 的 tip 码表(docs/design/leaderboard-system.md §4.5)。
//
// v1 **不开** rank 自己的 21000 段(L-14):全部错误复用 common 段 —— 参数非法与存储故障是跨域语义,
// 没有 rank 专属的码就没有开段的收益(开段还要同步 C++ 表工程三件套与客户端 gen_proto.ps1)。
// 第一个 rank 专属码出现的那一批再开段:在 Tip.xlsx **新插一行**组头 //rank_error base=21000 width=1000。
//
// 码由导表器发号(AGENTS §7 第 5 条),这里只引用生成的枚举,不写任何数字;constants_test.go 的
// TestNoHandWrittenTipCodes 机械守住。
package constants

import (
	"shared/generated/pb/table"
	"shared/serverbase"
)

const (
	// ErrInvalidParameter:取不到会话身份(无会话元数据的调用)、榜类型未知。业务拒绝,不是故障。
	//
	// 身份缺失刻意**不用** kPlayerNotFoundInSession(fault=1,契约 §3):请求根本没带会话属于客户端 / 链路问题,
	// 用故障码会把它刷成告警(与 friend / mail 同一理由)。
	ErrInvalidParameter = uint32(table.CommonError_kInvalidParameter)

	// ErrServiceUnavailable:RankRedis 读失败 / 读路径故障。**本包唯一的故障码**(Tip.xlsx fault 列为 1,
	// serverbase 记成 rpc_inband_fault 并配告警),所以绝不能拿它当"未上榜"或"榜是空的"的占位 ——
	// 未上榜是 self.rank = 0,空榜是 entries 为空,都不是错误(§4.2)。
	ErrServiceUnavailable = uint32(table.CommonError_kServiceUnavailable)
)

// TipClassifier 返回本服务的 in-band 业务码定性函数,供 serverbase.UnaryInterceptor 使用。
// 定性完全交给全局表(Tip.xlsx 的 fault 列 → tip.Faults),**不要**在这里包本地 map(AGENTS §7 第 5 条)。
func TipClassifier() serverbase.Classifier {
	return serverbase.TipVerdict
}
