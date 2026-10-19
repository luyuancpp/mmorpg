#pragma once

#include "entt/src/entt/entity/entity.hpp"
#include "engine/core/utils/registry/scene_entity.h"
#include "engine/core/type_define/type_define.h"

#include <cstdint>
#include <optional>

struct GrpcCallOutcome;
namespace scene_manager
{
class CreateSceneResponse;
}

class PlayerSceneSystem
{
public:
	// 同步通知不能把旧流程带进新的玩家/会话身份。未登记索引的独立 actor 允许保持未登记;
	// 生产入口另要求索引归本实体。requestSession 用于加载尚未绑定快照时守住预登记会话。
	class EntryOwnershipGuard
	{
	public:
		explicit EntryOwnershipGuard(entt::entity player, SessionId requestSession = 0);
		[[nodiscard]] bool IsCurrent() const;
	private:
		struct Snapshot
		{
			Guid playerId{0};
			entt::entity indexedPlayer{entt::null};
			SessionId sessionId{0};
			Guid snapshotPlayerId{0};
			std::optional<Guid> sessionOwner;
			SessionId loginSession{0};
			uint64_t loginGeneration{0};
			uint32_t homeZoneId{0};
			uint64_t ownerEpoch{0};
			bool operator==(const Snapshot&) const = default;
		};
		static std::optional<Snapshot> Read(entt::entity player, SessionId requestSession);
		entt::entity player_;
		SessionId requestSession_;
		std::optional<Snapshot> snapshot_;
	};

	// 只读解析业务场景号;不存在时返回 null,不把业务号当成本地句柄。
	[[nodiscard]] static SceneEntity FindSceneById(uint64_t sceneId);
	// 纯校验:场景须存在于本节点、身份非零;不改会话、归属、AOI 或场景绑定。
	// creators 只用于镜像自动进场回显,不承担准入语义(两版同基线,见 PARITY B5)。
	// GUID 重载供尚未创建实体的加载入口预检,实体重载还要求真实有效且非零的玩家身份。
	[[nodiscard]] static uint32_t CheckEnterScene(Guid playerId, SceneEntity scene);
	[[nodiscard]] static uint32_t CheckEnterScene(entt::entity player, SceneEntity scene);

	// true 表示已绑定目标场景(含幂等命中);输入无效或场景不存在返回 false。
	// 调用方只有成功后才可写登录态、触发后续落地逻辑。
	[[nodiscard]] static bool HandleEnterScene(entt::entity player, SceneEntity scene);

	// AOI 先用仍在的 Hex / SceneEntityComp 清理格子和观察者,再成对摘除场景绑定与 Hex。
	// 否则重连复用实体、落点 Hex 未变时不会重新入格;无效实体或不在场景里时 no-op。
	static void LeaveCurrentScene(entt::entity player);

	// RequestEnterMirrorScene asks SceneManager to create a mirror of the
	// player's current scene and then route the player into it. The
	// mirror is created with:
	//   - source_scene_id  = player's current scene_id (drives co-location;
	//     the new mirror lands on the same node when feasible)
	//   - mirror_config_id = caller-provided template id (>0; SceneInfoComp
	//     downstream filters mirrors via mirror_config_id() > 0)
	//   - creator_ids      = [player_id](镜像自动进场的创建者回显 / 归属提示;scene_manager 仍会回显,本节点按调用捕获身份认领,不用于准入)
	//   - scene_conf_id    = the source's scene_config_id (template) so the
	//     mirror inherits the same map definition
	//   - scene_type       = INSTANCE (mirrors are not main-world channels)
	//
	// 发出之前登记 PlayerMirrorCreateInFlightComp(令牌、发送时刻),走生成客户端的每次调用回调重载:
	// 完成回调带着 player_id 与令牌转到 HandleMirrorCreateDone,成功 / 业务失败 / 传输失败三条路径都能认领
	// 创建者。登记期间 PlayerLifecycleSystem::IsSceneChangeBusy 为真,别的换图 / 传送 / 跟随一律被挡。
	//
	// CreateScene 已发出时返回 true。战斗中、已有其他换图在途、当前场景或 SceneManager 不可用、
	// 玩家会话缺失时返回 false,且不登记在途状态;发起失败记录 WARN。
	//
	// mirrorConfigId must be > 0. Pass 0 only if the intent is "clone
	// everything from source" (no template), which goes through the
	// source-clone refusal path on SceneManager and is currently NOT
	// supported by this helper (callers always pick a mirror template).
	static bool RequestEnterMirrorScene(entt::entity player, uint32_t mirrorConfigId);

	// 镜像 CreateScene 的完成回调:RequestEnterMirrorScene 绑定的 GrpcDone 转到这里(单测直接调)。
	// 先按 playerId 找实体、按 token 认领 PlayerMirrorCreateInFlightComp,认领即摘;找不到实体或令牌对不上
	// (玩家已离开 / 实体已重建 / 已被顶替)就丢弃,不提示,建好的镜像由 scene_manager 按空场景回收。
	// 令牌匹配时先取发送时刻的 TTL 判据再摘槽;超过 deadline + 1s 或 UTC 回拨也只丢弃,不提示、不自动进场。
	// 后来的普通换图/交接/退出提交或权威落点成功已摘掉旧令牌;时钟恢复到 TTL 内也不会重新取得资格。
	// 认领后按结果分派:
	//   - 传输失败(结果未知):提示 kEnterSceneServerBusy,不断言"建镜像失败";
	//   - scene_manager 拒绝,或成功却回 scene_id 0:提示 kEnterSceneFailed;
	//   - 成功:提交点复核(不在战斗、没有别的换图 / 交接在途、scene_manager 可达)后,经
	//     PlayerLifecycleSystem::RequestSceneChange 发自动进场的 EnterScene,之后的 18 / 失败照普通换图处理。
	// EnterSceneC2S 早已回"已受理",仍有效而没进成的路径要给一条 tip(交接在途除外,交接自己会给结局),
	// 否则客户端一直等一个不会来的 EnterSceneS2C。
	// reply 只在 outcome.ok() 时非空,且只在调用期间有效(GrpcDone 契约)。
	static void HandleMirrorCreateDone(Guid playerId, uint64_t token, const GrpcCallOutcome& outcome,
									   const ::scene_manager::CreateSceneResponse* reply);

	// 把玩家当前场景的信息推给客户端(NotifySceneInfo,31 号)。SceneInfoC2S(43 号)的应答类型是 Empty,
	// 数据只走这条推送。withChannelDirectory(= 请求里的 with_channel_directory)为 true 且当前场景是
	// 大世界分线时,附带该地图的分线目录(SceneInfoS2C.channel_directory,docs/design/world-channel-switch.md §5):
	//   - 目录是 scene_manager 发布到 zone Redis 的读模型,这里只读转发,不做任何放行判断
	//     (切线请求仍由 scene_manager 在 EnterScene 里重新校验);
	//   - 读目录是一次异步 GET,所以大世界线上的推送晚于本函数返回;副本 / 镜像里、zone Redis
	//     未连接、或读命令发不出去时同步推送且不带目录;
	//   - withChannelDirectory 为 false 时同步推送、不读 Redis、不带目录,开销与加目录之前完全相同
	//     (压测机器人把 SceneInfoC2S 当高频动作在发,不能让它平白多出一次 Redis 往返);
	//   - 目录键不存在 / 读失败 / 解析失败 / 目录写的不是当前地图:照样推送,只是不带目录。
	//     客户端靠这条推送结束等待,所以"读不到目录"不能变成"不推"。
	// 不推送的情况只有:实体无效或玩家不在任何场景(记日志,与原 handler 行为一致);异步应答回来时
	// 实体已销毁、实体槽位已被别的玩家复用、或玩家已换到别的场景(客户端进新场景后会重新请求)。
	// 只在 scene 节点 loop 线程调用(hiredis 回调也在该线程)。由客户端请求触发,不在 per-tick 路径上。
	static void SendSceneInfo(entt::entity player, bool withChannelDirectory);
};
