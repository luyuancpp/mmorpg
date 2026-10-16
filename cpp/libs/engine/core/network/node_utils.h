#pragma once
#include <optional>
#include <vector>
#include <grpc/impl/connectivity_state.h>
#include "node/system/node/node_util.h"
#include "engine/core/type_define/type_define.h"

// 本节点身份的唯一所有者是 Node;底层模块只借用它的只读视图,不依赖 Node 的链接实现。
// Node 在构造时安装、完成停机并析构时卸载;未安装时返回 protobuf 只读默认实例,不创建状态。
// 安装/卸载仅用于所有者构造与排空后的析构(测试可用同线程夹具);访问限所属 loop 线程。
// 返回引用不得越过所有者寿命。
// 安装指针的原子发布不保护 NodeInfo 的并发读写,写入只经 Node::MutableNodeInfo()。
const NodeInfo &GetNodeInfo();
const NodeInfo *InstallLocalNodeInfo(const NodeInfo *info);
uint32_t GetZoneId();

// 为本玩家的请求挑一个 SceneManager 实例(换图、跨 zone 传送、镜像副本、紧急疏散、组队跟随都经这里)。
//
// - 候选必须同时拥有 NodeInfo、非空官方 channel、CQ 和未关闭的客户端寿命状态。
//   按状态分级：READY → IDLE / CONNECTING → TRANSIENT_FAILURE / SHUTDOWN。
// - 没有具备传输的候选时返回 entt::null，由调用方明确拒绝；不把只有 NodeInfo 的实体当成服务。
// - 有传输但全部不可达时仍确定性挑一个，让本次调用经 UNAVAILABLE/既有看门狗收尾。
//   不增加原地重挑或重发，保留失败代表「结果未知」的语义。
// - 候选集不变时同一玩家总落在同一实例;候选集一变可能换实例 —— scene_manager 没有按玩家存在单个
//   实例内存里的状态(请求去重、owner_epoch、交接标记都在 Redis),换实例不影响正确性。
// - 副作用:对每个挂了通道的候选调一次 GetState(try_to_connect=true),把 IDLE 的通道踢去重连;
//   落到兜底级时可能写一行告警日志。
// - 只在节点主循环线程调用(读 thread_local 注册表,告警节流状态也是 thread_local);不在 per-tick 路径上。
entt::entity GetSceneManagerEntity(Guid playerId);

namespace scene_manager_selector
{
	struct Candidate
	{
		entt::entity entity{entt::null};
		// 空 = 实体上没挂通道,不是某种连接状态。不要拿 GRPC_CHANNEL_SHUTDOWN 代替:两者分属不同的级,
		// 混成一级取模会让无通道实体在"还有 TRANSIENT_FAILURE 实例可兜底"时也可能被挑中。
		std::optional<grpc_connectivity_state> channelState;
	};

	// SceneManager 实例选择规则本体。纯函数:不碰注册表、不调 gRPC,单测直接喂状态
	// (cpp/tests/routing_identity_test/scene_manager_selector_test.cpp)。
	//
	//   ①  READY                                  首选
	//   ②  IDLE / CONNECTING                      没有 ① 时才用
	//   ③a 挂了通道,TRANSIENT_FAILURE / SHUTDOWN  ①② 都没有时兜底
	//   ③b 没挂通道(channelState 为空)            ①②③a 全空时才选
	//
	// 有 ①② 时绝不选 ③a / ③b;有 ③a 时绝不选 ③b。
	// 同一级内按打散后的 playerId 对该级实例数取模,序号沿用 candidates 的顺序;同一输入总得到同一实体。
	// 只有空表返回 entt::null,非空表一定返回其中一个实体。
	entt::entity Pick(const std::vector<Candidate> &candidates, Guid playerId);
}
