
#include "scene_handler.h"

///<<< BEGIN WRITING YOUR CODE

#include "infra/agones/agones_gameserver_lifecycle.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "node/system/node/node.h"
#include "player/system/player_lifecycle.h"
#include "player/system/player_scene.h"
#include "player/player_gm_guard.h" // 客户端 GM 指令统一闸门(guild-phase2 §S4 4.34)
#include "battle/system/player_battle.h"
#include "rpc/player_service_interface.h"
#include "network/rpc_channel.h"          // IsDispatchableMessageId:消息号越界或注册表空槽都不可派发
#include "network/rpc_channel_stats.h"    // ShouldLogRpcEvent
#include "network/rpc_controller.h"
#include "network/network_constants.h"
#include "network/error_handling_system.h"
#include "proto/common/component/player_async_comp.pb.h"
#include "proto/common/component/player_comp.pb.h" // UnregisterPlayer:退出中只放行 ExitGame
#include "proto/common/component/player_network_comp.pb.h"
#include "proto/common/base/node.pb.h"
#include "rpc/service_metadata/rpc_event_registry.h"
#include "type_alias/player_session_type_alias.h"
#include "core/utils/proto/proto_field_checker.h"
#include "network/network_utils.h"
#include "core/system/redis.h"
#include "player/comp/afk_comp.h"
#include "frame/manager/frame_time.h"
#include "rpc/service_metadata/player_lifecycle_service_metadata.h"
#include "proto/scene/scene_info.pb.h"
#include "modules/scene/scene_events.h"
#include <unordered_map>
#include <string>

using MessageUniquePtr = std::unique_ptr<google::protobuf::Message>;

using namespace muduo::net;

namespace
{

	// 源路由一族(Scene.RoutePlayerStringMsg / Scene.RouteNodeStringMsg,gate 侧的 RoutePlayerMessage /
	// RouteNodeMessage)已退役,只做显式拒绝。理由见 docs/design/muduo-rpc-channel.md §9.3:
	//   - 全仓没有发起端(C++ 只有这几个 handler 互相转发);RoutePlayerStringMsg(81)/ RoutePlayerMessage(74)
	//     不在 IsClientMessageId 里,客户端不可达;
	//   - 它是「源路由 + 逐跳同步镜像应答」:应答在下游完成之前就回给上游且没有错误字段,正是 A→B→C 的反例;
	//   - 旧的 scene 本地命中分支只打 TRACE、从不派发,gate→gate / scene→scene 分支拓扑不可达;
	//   - 跨节点玩家消息的权威通道是 Kafka 与持久收件箱(cross_scene_player_messaging.md、reliable-player-inbox-20261014.md)。
	// SetFailed 之后框架回 RPC_ERROR(HANDLER_FAILED),调用方得到 kRemoteFailed;这里不转发、不派发、不填 response。
	// proto 方法暂不删:message_id 还没有墓碑,删了会被发号器回收给新方法(§9.3 末段)。
	// 原先只为它们存在的 ResolveRoutedMessagePriority / GetMethodPriority / IsImportantPriority 一并删除。
	//
	// occurrences 由各 handler 的函数内 static 持有:handler 只在节点唯一的 EventLoop 线程上派发,不需要原子。
	// 日志按 ShouldLogRpcEvent 采样(前 8 条逐条,之后每 1024 条一条),防止对端刷包把日志打爆。
	void RejectDeprecatedRouteCall(::google::protobuf::RpcController *controller, const char *method,
								   uint64_t &occurrences)
	{
		controller->SetFailed(std::string(method) +
							  " is deprecated: no initiator; cross-node player messages go through Kafka / reliable inbox");
		++occurrences;
		if (!ShouldLogRpcEvent(occurrences))
		{
			return;
		}
		const auto &conn = RpcController::From(controller).conn();
		LOG_WARN << "Rejected deprecated RPC " << method
				 << ": no initiator; cross-node player messages go through Kafka / reliable inbox"
				 << " (docs/design/muduo-rpc-channel.md 9.3). peer="
				 << (conn ? conn->peerAddress().toIpPort() : std::string("<unknown>"))
				 << " occurrences=" << occurrences;
	}

	// 反向身份栅栏(docs/design/routing-identity-audit-20260908.md R13 的反向面)。
	//
	// scene 的 SessionMap 是 session_id -> player_id 的**本地缓存**,而 session_id 的高位
	// 嵌的是 gate 的 routing node_id —— 那个号是立刻复用的。gate G(node_id=3)重启后
	// 继任者 G' 仍拿 3 号并从头发 session 号,于是 G' 上属于 B 玩家的 session 5 和
	// scene 本地残留的"session 5 -> A 玩家"完全同号。既有的
	// `snapshot->gate_session_id() != sessionId` 复核只能发现"同一个玩家换了会话",
	// 对"同一个会话号换了玩家"完全失明:B 的客户端消息会被派发到 A 的实体上执行。
	//
	// 判据:发送方带了 player_id 就必须与 SessionMap 解析出的玩家一致。
	// 返回 true = 放行。0 = 老 gate(灰度窗口)放行,只在首次记一行 INFO。
	bool CheckRoutedPlayerIdentity(const char *where, const SessionId sessionId,
								   const uint64_t mappedPlayerId, const uint64_t claimedPlayerId,
								   const uint32_t messageId)
	{
		if (claimedPlayerId == 0)
		{
			// 迁移观测位:只打一次。逐条打等于把 scene 日志按客户端包速率放大。
			static bool logged = false;
			if (!logged)
			{
				logged = true;
				LOG_INFO << where << ": routed message without player_id (legacy gate). "
						 << "Reverse identity fence inactive for such messages; logged once. "
						 << "See docs/design/routing-identity-audit-20260908.md R13.";
			}
			return true;
		}
		if (mappedPlayerId == claimedPlayerId)
		{
			return true;
		}
		// 这条必须是 WARN:它意味着 gate 的 node_id 被复用(或 session 序号回绕)之后,
		// 有一条客户端消息本来会落到别的玩家身上。
		LOG_WARN << where << ": session/player mismatch, dropping routed message. session_id="
				 << sessionId << " mapped_player_id=" << mappedPlayerId
				 << " claimed_player_id=" << claimedPlayerId << " message_id=" << messageId;
		return false;
	}

	// 泛型应答按字段名写拒绝码;契约与生成层守卫见 services/player/rpc/player_service_interface.h。
	// 应答里没有这个字段(返回 Empty 的推送类方法)时返回 false、什么都不写。
	bool WriteRejectTip(google::protobuf::Message &playerResponse, const uint32_t tipId)
	{
		const auto *tipField = playerResponse.GetDescriptor()->FindFieldByName("error_message");
		if (tipField == nullptr ||
			tipField->cpp_type() != google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE ||
			tipField->message_type() != TipInfoMessage::descriptor())
		{
			return false;
		}
		TipInfoMessage tip;
		tip.set_id(tipId);
		playerResponse.GetReflection()->MutableMessage(&playerResponse, tipField)->CopyFrom(tip);
		return true;
	}

} // namespace

///<<< END WRITING YOUR CODE

void SceneHandler::PlayerEnterGameNode(::google::protobuf::RpcController* controller, const ::PlayerEnterGameNodeRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	LOG_DEBUG << "Handling PlayerEnterGameNode for player: " << request->player_id()
	          << " session=" << request->session_id()
	          << " enter_gs_type=" << request->enter_gs_type()
	          << " scene_id=" << request->scene_id()
	          << " login_generation=" << request->login_generation();

	// 入场的全部处置 —— 入场会话栅栏、改派待确认表对账、擦旧会话、两道 owner_epoch 废黜判定、复用实体或登记待入场
	// 上下文 —— 都在 PlayerLifecycleSystem::HandleEnterGameNode 里,顺序与理由见那里(scene-enter-session-fence.md §5.3)。
	// 这里只组上下文,并在需要新载入时发 AsyncLoad:AsyncLoad 留在节点层,scene 库的单测宿主没有 Redis 客户端,
	// 入口逻辑因此可以直接在宿主里驱动。
	PlayerEnterContext ctx;
	ctx.enterInfo.set_session_id(request->session_id());
	ctx.enterInfo.set_enter_gs_type(request->enter_gs_type());
	ctx.enterInfo.set_scene_id(request->scene_id());
	// 归属字段(gate 从 RoutePlayerEvent 透传):home_zone 决定落库 topic,owner_epoch 是存盘 CAS 的
	// 期望值。必须跟着"这一次路由决策"走,节点不得自己读 Redis(cross-zone-scene-travel.md CZ-3、
	// reentry-barrier §3.3)。0 = 上游未填,EnterScene 里不覆盖已有值。
	// 唯一例外(GO-2 §12.8 判定表 B5):源端 ResolveTravelOutcome 在交接被 scene_manager 单调回滚到本节点时采纳
	// Redis 里的 owner_epoch(E+2)。三条前提写死:同一段原子脚本先删掉本次交接这一族标记;location.rollback_receipt
	// 逐字节等于本次标记原文;location 指回本节点本 zone。别处新增"读 Redis 采纳 epoch"必须同时满足这三条。
	ctx.homeZoneId = request->home_zone_id();
	ctx.ownerEpoch = request->owner_epoch();
	// 登录代次(gate 用本会话、本角色见过的最高代次盖章):入场会话栅栏据此拒绝跨会话的陈旧入场。
	// 0 = 上游未带(旧版 gate / login),按判定表的版本 0 规则处理(scene-enter-session-fence.md §5.2 / §6)。
	ctx.loginGeneration = request->login_generation();

	if (PlayerLifecycleSystem::HandleEnterGameNode(request->player_id(), std::move(ctx)) !=
		PlayerLifecycleSystem::EnterGameNodeOutcome::kLoadRequired)
	{
		return;
	}

	// Player not online — kick off Redis-only async load.
	// Player data is expected to already be in Redis, preloaded by Login via
	// Kafka -> DB Service -> Redis. On Redis NIL, AsyncLoad retries with
	// millisecond-grade exponential backoff (500ms/1s/2s/4s/8s/16s) up to
	// kMaxLoadRetries; the first retry is fast so a healthy preload barely
	// impacts player-perceived login latency.
	// HandleEnterGameNode 已登记待入场上下文并发出 A2′(同一条 zone Redis 连接 FIFO,EVAL 排在这次 GET 前面)。
	tlsRedisSystem.GetPlayerDataRedis()->AsyncLoad(request->player_id());
	///<<< END WRITING YOUR CODE
}

void SceneHandler::SendMessageToPlayer(::google::protobuf::RpcController* controller, const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	LOG_TRACE << "Handling message routing for session ID: " << request->header().session_id()
			  << ", message ID: " << request->message_content().message_id();

	const auto it = SessionMap().find(request->header().session_id());
	if (it == SessionMap().end())
	{
		LOG_ERROR << "Session ID not found: " << request->header().session_id()
				  << ", message ID: " << request->message_content().message_id();
		return;
	}

	if (!CheckRoutedPlayerIdentity("SendMessageToPlayer", request->header().session_id(), it->second,
								   request->header().target_player_id(),
								   request->message_content().message_id()))
	{
		return;
	}

	const auto playerIt = tlsEcs.playerList.find(it->second);
	if (playerIt == tlsEcs.playerList.end())
	{
		LOG_ERROR << "Player ID not found in common logic: " << it->second;
		return;
	}

	const auto &player = playerIt->second;

	// Guard: reject messages from a superseded session (same as ProcessClientPlayerMessage).
	const auto *snapshot = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(player);
	if (snapshot && snapshot->gate_session_id() != request->header().session_id())
	{
		LOG_INFO << "SendMessageToPlayer: stale session " << request->header().session_id()
				 << " (current=" << snapshot->gate_session_id()
				 << "), dropping message_id=" << request->message_content().message_id();
		SessionMap().erase(it);
		return;
	}

	// 越界与注册表空槽(serviceName / methodName 为 nullptr,例如被跳过的 DB 域)都不可派发:
	// 只查越界会拿 nullptr 去构造 std::string(gPlayerService.find),对端发一帧就是 UB。
	if (!IsDispatchableMessageId(request->message_content().message_id()))
	{
		LOG_ERROR << "Invalid message ID: " << request->message_content().message_id();
		return;
	}

	const auto &messageInfo = gRpcMethodRegistry[request->message_content().message_id()];
	const auto serviceIt = gPlayerService.find(messageInfo.serviceName);
	if (serviceIt == gPlayerService.end())
	{
		LOG_ERROR << "PlayerService not found for message ID: " << request->message_content().message_id();
		return;
	}

	const auto &serviceHandler = serviceIt->second;
	google::protobuf::Service *service = serviceHandler->service();
	const google::protobuf::MethodDescriptor *method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
	if (method == nullptr)
	{
		LOG_ERROR << "Method not found in PlayerService: " << messageInfo.methodName;
		return;
	}

	const MessageUniquePtr playerRequest(service->GetRequestPrototype(method).New());
	if (!playerRequest->ParsePartialFromArray(request->message_content().serialized_message().data(),
											  int32_t(request->message_content().serialized_message().size())))
	{
		LOG_ERROR << "Failed to parse request message for message ID: " << request->message_content().message_id()
				  << " session=" << request->header().session_id();
		return;
	}

	std::string errorDetails;
	if (ProtoFieldChecker::CheckFieldSizes(*playerRequest, kProtoFieldCheckerThreshold, errorDetails))
	{
		LOG_ERROR << errorDetails << " message_id=" << request->message_content().message_id()
				  << " session=" << request->header().session_id();
		return;
	}
	if (ProtoFieldChecker::CheckForNegativeInts(*playerRequest, errorDetails))
	{
		LOG_ERROR << errorDetails << " message_id=" << request->message_content().message_id()
				  << " session=" << request->header().session_id();
		return;
	}

	const MessageUniquePtr playerResponse(service->GetResponsePrototype(method).New());
	serviceHandler->CallMethod(method, player, playerRequest.get(), playerResponse.get());

	response->mutable_header()->set_session_id(request->header().session_id());
	response->mutable_message_content()->set_message_id(request->message_content().message_id());

	if (Empty::GetDescriptor() == playerResponse->GetDescriptor())
	{
		return;
	}

	response->mutable_message_content()->set_serialized_message(playerResponse->SerializeAsString());
	///<<< END WRITING YOUR CODE
}

void SceneHandler::ProcessClientPlayerMessage(::google::protobuf::RpcController* controller, const ::ProcessClientPlayerMessageRequest* request,
	::ProcessClientPlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	if (!request)
		return;

	const auto &msg = request->message_content();
	const SessionId sessionId = request->session_id();

	// 越界与注册表空槽都按「message_id not found」处理(见 SendMessageToPlayer 同一判据的注释)。
	if (!IsDispatchableMessageId(msg.message_id()))
	{
		LOG_ERROR << "ProcessClientPlayerMessage: message_id not found " << msg.message_id();
		return;
	}

	const auto &messageInfo = gRpcMethodRegistry.at(msg.message_id());
	const auto serviceIt = gPlayerService.find(messageInfo.serviceName);
	if (serviceIt == gPlayerService.end())
	{
		LOG_ERROR << "ProcessClientPlayerMessage: player service not found for message id " << msg.message_id();
		return;
	}

	google::protobuf::Service *service = serviceIt->second->service();
	const google::protobuf::MethodDescriptor *method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
	if (method == nullptr)
	{
		LOG_ERROR << "ProcessClientPlayerMessage: method not found " << messageInfo.methodName;
		return;
	}

	const auto it = SessionMap().find(sessionId);
	if (it == SessionMap().end())
	{
		LOG_ERROR << "ProcessClientPlayerMessage: session id not found " << sessionId << ", message id " << msg.message_id();
		return;
	}

	if (!CheckRoutedPlayerIdentity("ProcessClientPlayerMessage", sessionId, it->second,
								   request->player_id(), msg.message_id()))
	{
		return;
	}

	const auto player = tlsEcs.GetPlayer(it->second);
	if (player == entt::null)
	{
		// Player entity not yet created (async Redis load in progress).
		// If this is an ExitGame request, erase the pre-registered session
		// so HandlePlayerAsyncLoaded knows the load was cancelled.
		if (msg.message_id() == ScenePlayerExitGameMessageId)
		{
			LOG_INFO << "ProcessClientPlayerMessage: ExitGame during async load, "
						"cancelling session for player_id=" << it->second;
			SessionMap().erase(it);
		}
		else
		{
			LOG_WARN << "ProcessClientPlayerMessage: player not loaded for player_id=" << it->second
					 << " message_id=" << msg.message_id() << " (likely still loading from Redis)";
		}
		return;
	}

	// Guard: reject messages from a superseded session.
	// After reconnect, SessionMap may briefly contain both old and new entries
	// pointing to the same player_id.  If a late message (especially ExitGame)
	// arrives on the OLD session, it must NOT be dispatched to the player.
	const auto *snapshot = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(player);
	if (snapshot && snapshot->gate_session_id() != sessionId)
	{
		LOG_INFO << "ProcessClientPlayerMessage: stale session " << sessionId
				 << " (current=" << snapshot->gate_session_id()
				 << ") for player_id=" << it->second
				 << ", dropping message_id=" << msg.message_id();
		SessionMap().erase(it);
		return;
	}

	// Update last-active frame for AFK detection.
	// 退出中(UnregisterPlayer)的实体不续:它的消息除 ExitGame 外都会被下面的退出闸拒掉,被拒的消息不算"活跃",
	// 否则仍连着的客户端(停机 / 排空这类服务端发起的退出)每发一条被拒的消息都在续 AFK 计时。
	if (!tlsEcs.actorRegistry.any_of<UnregisterPlayer>(player))
	{
		tlsEcs.actorRegistry.get<LastActiveFrameComp>(player).frame =
			tlsFrameTimeManager.frameTime.current_frame();
	}

	// Parse request and validate field constraints
	const MessageUniquePtr playerRequest(service->GetRequestPrototype(method).New());
	if (!playerRequest->ParsePartialFromArray(msg.serialized_message().data(),
											  static_cast<int32_t>(msg.serialized_message().size())))
	{
		LOG_ERROR << "ProcessClientPlayerMessage: failed to parse client-sent request message_id=" << msg.message_id()
				  << " session=" << sessionId;
		return;
	}

	std::string errorDetails;
	if (ProtoFieldChecker::CheckFieldSizes(*playerRequest, kProtoFieldCheckerThreshold, errorDetails))
	{
		LOG_ERROR << "ProcessClientPlayerMessage: " << errorDetails << " message_id=" << msg.message_id()
				  << " session=" << sessionId;
		return;
	}
	if (ProtoFieldChecker::CheckForNegativeInts(*playerRequest, errorDetails))
	{
		LOG_ERROR << "ProcessClientPlayerMessage: " << errorDetails << " message_id=" << msg.message_id()
				  << " session=" << sessionId;
		return;
	}

	// Dispatch to the concrete player service method
	const MessageUniquePtr playerResponse(service->GetResponsePrototype(method).New());
	// 落成 std::string 再用:MethodDescriptor::name() 返回 absl::string_view
	// (descriptor.h:1354),它既没有 c_str(),muduo 的 LogStream 也没有对应的
	// operator<<。拷一次的代价只发生在闸门命中之前的一次判定上。
	const std::string methodName(method->name());
	if (tlsEcs.actorRegistry.any_of<UnregisterPlayer>(player) && msg.message_id() != ScenePlayerExitGameMessageId)
	{
		// 退出中(UnregisterPlayer:退出存盘在途 / 等收敛)的实体只放行 ExitGame
		// (cross-zone-scene-travel.md §12.6.3 第一步,M1 第 2 条)。退出分支要等"刚落地的就是当前内存"
		// 才销毁,这里不关门的话,停机 / 排空期间照收的客户端操作会一直改实体、退出永不收敛;
		// 而落地后才到的改动,在"落地即销毁"的旧假设下是直接丢掉的。
		// 回 kFeatureUnavailable(common 段现成的业务拒绝码,"该功能当前不可用",非故障):
		// tip 表里没有"正在退出"专用码,也不为此新增 —— 这条路上的客户端多半已经断开(gate 断线即代发 ExitGame),
		// 仍连着的只有停机 / 排空这类服务端发起的退出,对它们"当前不可用"是如实的。
		// 逐条只打 DEBUG(断线瞬间客户端在途的包都会走到这里),趋势看 [ExitPersist] client_msg_rejected。
		exit_persist_stats::Inc(exit_persist_stats::Get().clientMsgRejected);
		LOG_DEBUG << "ProcessClientPlayerMessage: player_id=" << it->second
				  << " is exiting; dropping message_id=" << msg.message_id() << " method=" << methodName;
		WriteRejectTip(*playerResponse, kFeatureUnavailable);
	}
	else if (scene_gm_guard::IsClientGmMethodName(methodName) &&
		scene_gm_guard::RejectGmClientRpc(methodName.c_str()))
	{
		// 客户端 GM 指令统一闸门(guild-phase2.md §S4 4.34)。
		//
		// 这里是客户端消息进 scene 的**唯一**入口:gate 发
		// SceneProcessClientPlayerMessageMessageId(client_message_processor.cpp:699-711)
		// 只走到这个函数。逐 handler 加闸必漏 —— P0-a 就漏掉了
		// SceneRollbackClientPlayer 的十二条(GmExecuteRollback / GmClawbackItem 等):
		// 它们的 service 没标 OptionIsClientProtocolService,gate 的清单挡得住,
		// 但本函数不看这个 option,直连 scene RPC 端口照样能把 message_id 102-117
		// 打进来,而那几条今天还是空桩,一旦实现就是客户端自助回档。
		//
		// 判据与 P0-a 同一套(SCENE_RUN_MODE,默认 prod = 拒绝,见
		// cpp/nodes/gate/SECURITY.md §3),不另起一个环境变量 —— 同一件事两个开关
		// 只会让运维在拒绝时不知道该看哪个。
		//
		// InvokePlayerService(下面那个)与 SendMessageToPlayer 是**节点路由**,不对
		// 客户端开放,所以不在这里加闸;它们那侧的防护是各 Gm* handler 自己的
		// scene_gm_guard::RejectGmClientRpc。将来若再有路径转发客户端消息,必须调
		// 同一个谓词。
		LOG_WARN << "ProcessClientPlayerMessage: client GM rejected method=" << methodName
				 << " player_id=" << it->second << " session=" << sessionId;

		// GM 闸不走 CallMethod,与退出闸共用 WriteRejectTip 直接写本次应答。
		// Empty 应答没有 error_message 字段,不写。
		WriteRejectTip(*playerResponse, kFeatureUnavailable);
	}
	else
	{
		serviceIt->second->CallMethod(method, player, playerRequest.get(), playerResponse.get());
	}

	// Populate synchronous response (if caller expects one)
	if (response != nullptr && Empty::GetDescriptor() != playerResponse->GetDescriptor())
	{
		response->mutable_message_content()->set_message_id(msg.message_id());
		response->mutable_message_content()->set_id(msg.id());
		response->set_session_id(sessionId);

		// Serialize response
		const auto byte_size = playerResponse->ByteSizeLong();
		response->mutable_message_content()->mutable_serialized_message()->resize(byte_size);
		if (!playerResponse->SerializePartialToArray(response->mutable_message_content()->mutable_serialized_message()->data(),
													 static_cast<int32_t>(byte_size)))
		{
			LOG_ERROR << "ProcessClientPlayerMessage: Failed to serialize response for message ID: " << msg.message_id();
			// Optionally fill error message here
		}
	}

	///<<< END WRITING YOUR CODE
}

void SceneHandler::InvokePlayerService(::google::protobuf::RpcController* controller, const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const auto it = SessionMap().find(request->header().session_id());
	if (it == SessionMap().end())
	{
		LOG_ERROR << "session id not found " << request->header().session_id() << ","
				  << " message id " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kSessionNotFound);
		return;
	}

	if (!CheckRoutedPlayerIdentity("InvokePlayerService", request->header().session_id(), it->second,
								   request->header().target_player_id(),
								   request->message_content().message_id()))
	{
		return;
	}

	const auto player = tlsEcs.GetPlayer(it->second);
	if (player == entt::null)
	{
		LOG_ERROR << "GatePlayerService player not loading";
		SendErrorToClient(*request, *response, kPlayerNotFoundInSession);
		return;
	}

	// Guard: reject messages from a superseded session.
	const auto *snapshot = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(player);
	if (snapshot && snapshot->gate_session_id() != request->header().session_id())
	{
		LOG_INFO << "InvokePlayerService: stale session " << request->header().session_id()
				 << " (current=" << snapshot->gate_session_id()
				 << "), dropping message_id=" << request->message_content().message_id();
		SessionMap().erase(it);
		return;
	}

	// 越界与注册表空槽都按「message_id not found」处理(见 SendMessageToPlayer 同一判据的注释)。
	if (!IsDispatchableMessageId(request->message_content().message_id()))
	{
		LOG_ERROR << "message_id not found " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kMessageIdNotFound);
		return;
	}

	const auto &messageInfo = gRpcMethodRegistry[request->message_content().message_id()];
	const auto serviceIt = gPlayerService.find(messageInfo.serviceName);
	if (serviceIt == gPlayerService.end())
	{
		LOG_ERROR << "PlayerService service not found " << request->header().session_id()
				  << "," << request->message_content().message_id();
		return;
	}

	const auto &serviceHandler = serviceIt->second;
	google::protobuf::Service *service = serviceHandler->service();
	const google::protobuf::MethodDescriptor *method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
	if (method == nullptr)
	{
		LOG_ERROR << "PlayerService method not found " << request->message_content().message_id();
		return;
	}

	MessageUniquePtr playerRequest(service->GetRequestPrototype(method).New());
	if (!playerRequest->ParsePartialFromArray(request->message_content().serialized_message().data(), int32_t(request->message_content().serialized_message().size())))
	{
		LOG_ERROR << "ParsePartialFromArray " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kRequestMessageParseError);
		return;
	}

	std::string errorDetails;

	// Validate field sizes
	if (ProtoFieldChecker::CheckFieldSizes(*playerRequest, kProtoFieldCheckerThreshold, errorDetails))
	{
		LOG_ERROR << errorDetails << " Failed to check request for message ID: "
				  << request->message_content().message_id();
		SendErrorToClient(*request, *response, kArraySizeTooLargeInMessage);
		return;
	}

	// Validate no negative integers
	if (ProtoFieldChecker::CheckForNegativeInts(*playerRequest, errorDetails))
	{
		LOG_ERROR << errorDetails << " Failed to check request for message ID: "
				  << request->message_content().message_id();
		SendErrorToClient(*request, *response, kNegativeValueInMessage);
		return;
	}

	MessageUniquePtr playerResponse(service->GetResponsePrototype(method).New());
	serviceHandler->CallMethod(method, player, playerRequest.get(), playerResponse.get());

	response->mutable_header()->set_session_id(request->header().session_id());
	response->mutable_message_content()->set_message_id(request->message_content().message_id());

	if (Empty::GetDescriptor() == playerResponse->GetDescriptor())
	{
		return;
	}

	const auto byte_size = playerResponse->ByteSizeLong();
	response->mutable_message_content()->mutable_serialized_message()->resize(byte_size);
	if (!playerResponse->SerializePartialToArray(response->mutable_message_content()->mutable_serialized_message()->data(),
												 static_cast<int32_t>(byte_size)))
	{
		LOG_ERROR << "Failed to serialize response for message ID: " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kResponseMessageParseError);
		return;
	}

	///<<< END WRITING YOUR CODE
}

void SceneHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller, const ::RouteMessageRequest* request,
	::RouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	// 已退役:显式拒绝,不再转发(见全局守护段 RejectDeprecatedRouteCall 与 docs/design/muduo-rpc-channel.md §9.3)。
	static uint64_t rejectedCalls = 0;
	RejectDeprecatedRouteCall(controller, "Scene.RouteNodeStringMsg", rejectedCalls);
	///<<< END WRITING YOUR CODE
}

void SceneHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller, const ::RoutePlayerMessageRequest* request,
	::RoutePlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	// 已退役:显式拒绝。不再按 node_list 转发、不再镜像 response;跨 zone 冻结闸等本地分支随之退场
	// (见全局守护段 RejectDeprecatedRouteCall 与 docs/design/muduo-rpc-channel.md §9.3)。
	static uint64_t rejectedCalls = 0;
	RejectDeprecatedRouteCall(controller, "Scene.RoutePlayerStringMsg", rejectedCalls);
	///<<< END WRITING YOUR CODE
}

void SceneHandler::UpdateSessionDetail(::google::protobuf::RpcController* controller, const ::RegisterPlayerSessionRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	// 入场会话栅栏(scene-enter-session-fence.md §5.8):本 RPC 无条件改绑会话、不带登录代次,按代次 0 过同一栅栏 ——
	// 实体已有代次且会话不同即拒(kRejectUnversioned)。非生成代码里没有发送方,但消息号可达,不能留成绕过栅栏的口子。
	// 拒绝时什么都不碰(不摘当前会话)。
	const enter_fence::Verdict verdict =
		PlayerLifecycleSystem::JudgeEnter(request->player_id(), request->session_id(), /*loginGeneration=*/0);
	if (!enter_fence::Admits(verdict))
	{
		enter_fence_stats::Inc(enter_fence_stats::Get().sessionUpdateRejected);
		LOG_WARN << "[EnterFence] UpdateSessionDetail rejected player=" << request->player_id()
				 << " session=" << request->session_id() << " verdict=" << enter_fence::VerdictName(verdict)
				 << " (metric=session_update_rejected)";
		return;
	}

	// 只读校验全部做完再动会话,与上面栅栏拒绝同口径:任何一项不满足都什么都不碰。
	// 以前先 RemovePlayerSession 再查 gate / 玩家:gate 查不到时旧会话已经摘掉,玩家停在无会话状态;
	// 快照组件缺失时还会走到无守卫的 get(ecs-entt-misuse-audit-20261015 F094)。
	if (!ResolveLocalZoneGateEntity(request->session_id()))
	{
		LOG_ERROR << "Gate not found " << GetGateNodeId(request->session_id());
		return;
	}

	const auto player = tlsEcs.GetPlayer(request->player_id());
	if (!tlsEcs.actorRegistry.valid(player))
	{
		LOG_ERROR << "Player not found " << request->player_id();
		return;
	}

	// PlayerSessionSnapshotComp 只在入场流程绑定会话时创建(player_lifecycle.cpp,会话号非 0 才建),
	// 不带会话入场的玩家没有它。缺失说明这个玩家从未绑定过会话:拒绝改绑,不在这里补建。
	auto *sessionSnapshot = tlsEcs.actorRegistry.try_get<PlayerSessionSnapshotComp>(player);
	if (sessionSnapshot == nullptr)
	{
		LOG_ERROR << "UpdateSessionDetail rejected: PlayerSessionSnapshotComp missing, player=" << request->player_id()
				  << " entity=" << entt::to_integral(player) << " session=" << request->session_id();
		return;
	}

	PlayerLifecycleSystem::RemovePlayerSession(player);

	// 与入场绑定会话(player_lifecycle.cpp)同口径用 insert_or_assign:gate 同 node_id 重启后会话号从头发,
	// 这个号可能还挂着别的玩家的旧映射。emplace 遇到已有键不覆盖,映射会停在旧玩家身上,本玩家快照却写成新号,
	// 两边不一致。旧玩家那边擦映射时只擦仍归自己的(EraseSessionMappingIfOwned),不会反过来擦掉这条
	// (scene-enter-session-fence.md §5.6)。
	SessionMap().insert_or_assign(request->session_id(), request->player_id());

	// RemovePlayerSession 与 SessionMap 只改字段和映射,不增删 actorRegistry 的组件,sessionSnapshot 仍然有效。
	sessionSnapshot->set_gate_session_id(request->session_id());
	// 改绑后同步登录身份的会话号(代次按 0 传 = 保持原值):之后到达的陈旧入场拿它比,而不是拿已被改掉的旧会话。
	PlayerLifecycleSystem::RecordLoginIdentity(player, request->session_id(), /*loginGeneration=*/0);

	PlayerLifecycleSystem::HandleBindPlayerToGateOK(player);
	///<<< END WRITING YOUR CODE
}

void SceneHandler::CreateScene(::google::protobuf::RpcController* controller, const ::CreateSceneRequest* request,
	::CreateSceneResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	if (request->config_id() == 0 || request->scene_id() == 0)
	{
		LOG_ERROR << "CreateScene: config_id=" << request->config_id()
				  << " scene_id=" << request->scene_id() << ", both must be non-zero";
		return;
	}

	// Agones high-density mode: allocate gate.
	//
	// This handler runs ON the muduo EventLoop, so the blocking variant is NOT
	// allowed here -- one HTTP hiccup would stall the whole logic frame. The
	// non-blocking variant kicks an async allocate and returns false right
	// away; this create fails closed and the caller's retry normally lands
	// after the process is already Allocated. (SceneManager uses the gRPC
	// path, which does get the bounded blocking variant.)
	auto createPermit = agones::GameServerLifecycle::Instance().AcquireAllocationPermitNonBlocking();
	if (!createPermit)
	{
		LOG_ERROR << "CreateScene: rejected, Agones allocate not confirmed yet (state="
				  << agones::ToString(agones::GameServerLifecycle::Instance().State())
				  << "), scene_id=" << request->scene_id() << "; retry expected";
		return;
	}

	// Idempotent: deduplicate by scene_id (Go-allocated, globally unique).
	{
		auto view = tlsEcs.sceneRegistry.view<SceneInfoComp>();
		for (auto entity : view)
		{
			const auto &info = view.get<SceneInfoComp>(entity);
			if (info.scene_id() == request->scene_id())
			{
				response->mutable_scene_info()->CopyFrom(info);
				LOG_INFO << "CreateScene: scene_id=" << request->scene_id()
						 << " already exists (entity=" << entt::to_integral(entity) << "), returning existing";
				return;
			}
		}
	}

	auto sceneEntity = tlsEcs.sceneRegistry.create();

	// Keep this block in sync with SceneNodeGrpcImpl::HandleCreateScene. Both
	// entrypoints write the same fields so the mirror-co-location pipeline
	// works no matter which transport the caller uses.
	//
	// sceneInfo 引用只在这个块里用。下面的 dispatcher.trigger 会同步跑监听者,监听者一旦删掉场景
	// 或 SceneInfoComp(swap-and-pop),trigger 之前拿的引用就悬空了(ecs-entt-misuse-audit-20261015 F096)。
	{
		auto &sceneInfo = tlsEcs.sceneRegistry.emplace<SceneInfoComp>(sceneEntity);
		sceneInfo.set_scene_config_id(request->config_id());
		sceneInfo.set_scene_id(request->scene_id());
		sceneInfo.set_mirror_config_id(request->mirror_config_id());
		sceneInfo.set_dungeon_config_id(request->dungeon_config_id());

		for (const auto creator_id : request->creator_ids())
		{
			sceneInfo.mutable_creators()->insert({creator_id, true});
		}
	}


	OnSceneCreated event;
	event.scene = sceneEntity;
	tlsEcs.dispatcher.trigger(event);

	// 回包按监听者跑完之后的状态重新取。场景在 trigger 里被删掉时不回 scene_info,
	// 和上面的校验失败分支同一口径。
	const auto *createdInfo = tlsEcs.sceneRegistry.try_get<SceneInfoComp>(sceneEntity);
	if (createdInfo == nullptr)
	{
		LOG_ERROR << "CreateScene: scene entity=" << entt::to_integral(sceneEntity)
				  << " scene_id=" << request->scene_id()
				  << " lost SceneInfoComp inside OnSceneCreated listeners, not reporting success";
		// 回滚半成品场景,做成真正的 fail-closed。OnSceneCreated 已经发出,监听者按「场景已建成」记过账
		// (Agones 计数、AOI 格子表)。不收掉的话,这个实体没有 SceneInfoComp,按 scene_id 去重的 view 看不到它,
		// 调用方重试会再建一个,它就永远留在 sceneRegistry 里。先补发 OnSceneDestroyed 让监听者对称撤账,
		// 再销毁实体,与 DestroyScene 的收尾顺序一致;刚建的场景没有住户,不需要先排空。
		// 实体已被监听者自己销毁时,撤账归那个监听者,这里不再重复。
		if (tlsEcs.sceneRegistry.valid(sceneEntity))
		{
			OnSceneDestroyed rollbackEvent;
			rollbackEvent.scene = sceneEntity;
			tlsEcs.dispatcher.trigger(rollbackEvent);
			tlsEcs.sceneRegistry.destroy(sceneEntity);
		}
		return;
	}
	response->mutable_scene_info()->CopyFrom(*createdInfo);

	LOG_INFO << "CreateScene: created scene entity=" << entt::to_integral(sceneEntity)
			 << " config_id=" << request->config_id()
			 << " scene_id=" << createdInfo->scene_id()
			 << " mirror_config_id=" << createdInfo->mirror_config_id()
			 << " dungeon_config_id=" << createdInfo->dungeon_config_id();
	///<<< END WRITING YOUR CODE
}

void SceneHandler::DestroyScene(::google::protobuf::RpcController* controller, const ::DestroySceneRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const auto sceneId = request->scene_id();
	if (sceneId == 0)
	{
		LOG_ERROR << "DestroyScene: scene_id is 0";
		return;
	}

	// Find the scene entity by scene_id.
	SceneEntity targetEntity = entt::null;
	auto view = tlsEcs.sceneRegistry.view<SceneInfoComp>();
	for (auto entity : view)
	{
		const auto &info = view.get<SceneInfoComp>(entity);
		if (info.scene_id() == sceneId)
		{
			targetEntity = entity;
			break;
		}
	}

	if (targetEntity == entt::null)
	{
		LOG_WARN << "DestroyScene: scene entity not found for scene_id=" << sceneId;
		return;
	}

	// Drain-then-destroy. Keep in sync with SceneNodeGrpcImpl::HandleDestroyScene
	// -- see the long comment there for why we return without destroying.
	if (const std::size_t relocating = PlayerLifecycleSystem::BeginSceneDrain(targetEntity); relocating > 0)
	{
		LOG_WARN << "DestroyScene: scene_id=" << sceneId << " still has residents; relocating "
				 << relocating << " player(s) to the main world, entity kept until drained";
		return;
	}

	// Fire scene-destroyed event before entity removal.
	OnSceneDestroyed event;
	event.scene = targetEntity;
	tlsEcs.dispatcher.trigger(event);

	// Destroy the ECS entity (removes all components).
	tlsEcs.sceneRegistry.destroy(targetEntity);

	LOG_INFO << "DestroyScene: destroyed scene entity for scene_id=" << sceneId;
	///<<< END WRITING YOUR CODE}
}

void SceneHandler::PrepareBattle(::google::protobuf::RpcController* controller, const ::PrepareBattleRequest* request,
	::PrepareBattleResponse* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	// match -> scene 备战冻结(muduo RPC 入口,已在 loop 线程,直接委托;
	// match Go 侧实际走 SceneNodeGrpc 的 gRPC 面,本面供 C++ 节点间调用)
	PlayerBattleSystem::PrepareBattle(*request, *response);
///<<< END WRITING YOUR CODE}
}

void SceneHandler::CancelBattlePrepare(::google::protobuf::RpcController* controller, const ::CancelBattlePrepareRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	// gather 失败补偿解冻,幂等
	PlayerBattleSystem::CancelBattlePrepare(*request);
///<<< END WRITING YOUR CODE}
}

void SceneHandler::NodeHandshake(::google::protobuf::RpcController* controller, const ::NodeHandshakeRequest* request,
	::NodeHandshakeResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	gNode->GetNodeRegistrationManager().OnNodeHandshake(RpcController::From(controller), *request, *response);
	///<<< END WRITING YOUR CODE
}
