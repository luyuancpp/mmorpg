
#include "centre_service_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "centre_node.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "table/code/mainscene_table.h"
#include "session/system/session.h"
#include "rpc/player_service_interface.h"
#include "muduo/net/Callbacks.h"
#include "network/network_constants.h"
#include "network/rpc_session.h"
#include "network/error_handling_system.h"
#include "table/proto/tip/login_error_tip.pb.h"
#include "player/system/player_lifecycle.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "proto/common/node.pb.h"
#include "scene/system/player_change_room.h"
#include "rpc/service_metadata/scene_service_metadata.h"
#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "rpc/service_metadata/service_metadata.h"
#include "threading/redis_manager.h"
#include "type_alias/player_session_type_alias.h"
#include "core/utils/defer/defer.h"
#include "core/utils/proto/proto_field_checker.h"
#include "core/utils/debug/stacktrace_system.h"
#include "player/system/player_tip.h"
#include "rpc/service_metadata/centre_player_scene_service_metadata.h"
#include "type_alias/player_redis.h"
#include "network/network_utils.h"
#include "network/player_message_utils.h"
#include "threading/node_context_manager.h"
#include "threading/player_manager.h"
#include "threading/message_context.h"
#include <modules/scene/comp/room_node_comp.h>
#include <scene/system/room.h>
#include <time/system/time.h>
#include <utils/hash/sha.h>

using namespace muduo;
using namespace muduo::net;

struct DelayedCleanupTimer {
	TimerTaskComp timer;
};

constexpr std::size_t kMaxPlayerSize{ 50000 };

extern std::unordered_map<std::string, std::unique_ptr<::google::protobuf::Service>> gNodeService;

void StartDelayedCleanupTimer(entt::entity playerEntity, uint32_t timeoutMs) {
	tlsRegistryManager.actorRegistry.get_or_emplace<DelayedCleanupTimer>(playerEntity).timer.RunAfter(static_cast<double>(timeoutMs) / 1000.0,
		[playerEntity]() {
			auto* sessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
			auto* disconnectInfo = tlsRegistryManager.actorRegistry.try_get<PlayerDisconnectInfoComp>(playerEntity);
			if (!sessionPB || !disconnectInfo) return;
			if (sessionPB->session_version() != disconnectInfo->snapshot_version()) {
				// 已经重连/被替换，取消清理
				return;
			}
			// 仍为断线状态：先移除全局 session 映射，再真正下线
			const uint64_t gateSessionId = sessionPB->gate_session_id();
			if (gateSessionId != kInvalidSessionId) {
				GlobalSessionList().erase(gateSessionId);
				LOG_INFO << "Delayed cleanup: removed GlobalSessionList entry for session " << gateSessionId;
			}
			Guid playerGuid = tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity);
			PlayerLifecycleSystem::HandleNormalExit(playerGuid);
		});
}

// 新增：取消延迟清理计时器的辅助函数（匿名 namespace 内）
static void CancelDelayedCleanupTimer(entt::entity playerEntity) {
	auto& registry = tlsRegistryManager.actorRegistry;
	if (!registry.valid(playerEntity)) return;
	if (registry.try_get<DelayedCleanupTimer>(playerEntity)) {
		registry.remove<DelayedCleanupTimer>(playerEntity);
		// 记录日志：使用 player GUID 更易读
		if (registry.try_get<Guid>(playerEntity)) {
			LOG_INFO << "Cancelled delayed cleanup timer for player " << registry.get<Guid>(playerEntity);
		} else {
			LOG_INFO << "Cancelled delayed cleanup timer for entity " << static_cast<uint32_t>(playerEntity);
		}
	}
}

Guid GetPlayerIDBySessionId(const uint64_t session_id)
{
	const auto session_it = GlobalSessionList().find(session_id);
	if (session_it == GlobalSessionList().end())
	{
		LOG_DEBUG << "Cannot find session ID " << session_id << GetStackTraceAsString();
		return kInvalidGuid;
	}
	return session_it->second;
}

entt::entity GetPlayerEntityBySessionId(uint64_t session_id)
{
	auto player_id = GetPlayerIDBySessionId(session_id);

	LOG_TRACE << "Getting player entity for session ID: " << session_id << ", player ID: " << player_id;

	const auto player_it = tlsPlayerList.find(player_id);
	if (player_it == tlsPlayerList.end())
	{
		LOG_ERROR << "Player not found for session ID: " << session_id << ", player ID: " << player_id;
		return entt::null;
	}

	LOG_TRACE << "Player entity found for session ID: " << session_id << ", player ID: " << player_id;

	return player_it->second;
}


// ---------- refactor helpers (匿名 namespace) ----------
// helpers for login / reconnect / kick decision (single-threaded simplified)
namespace {

	enum class EnterGameDecision {
		FirstLogin,
		Reconnect,
		ReplaceLogin
	};

	// 修改：DecideEnterGame 增加 oldTokenValid 参数
	EnterGameDecision DecideEnterGame(bool hasOldSession,
		const std::string& oldLoginToken,
		const std::string& newLoginToken,
		bool oldTokenValid)
	{
		if (!hasOldSession) return EnterGameDecision::FirstLogin;
		// 只有在旧 token 未过期且与新 token 相同的情况下认为是 Reconnect
		if (oldTokenValid && oldLoginToken == newLoginToken) return EnterGameDecision::Reconnect;
		return EnterGameDecision::ReplaceLogin;
	}

	// 发送 kick（携带期望版本）
	bool SendKickForOldSession(uint64_t oldSessionId, uint64_t expectedVersion)
	{
		if (oldSessionId == kInvalidSessionId) return false;
		KickSessionRequest kickMsg;
		kickMsg.set_session_id(oldSessionId);
		kickMsg.set_expected_session_version(expectedVersion);
		SendMessageToGateById(GateKickSessionByCentreMessageId, kickMsg, GetGateNodeId(oldSessionId));
		LOG_INFO << "Requested kick for old session: " << oldSessionId << " expected_version=" << expectedVersion;
		return true;
	}

	// 更新/创建 player 的 session snapshot（写入新的 sessionId + token）
	// 返回旧的 sessionId 与旧 token（用于判定与后续清理）
// 更新 snapshot，递增 version 并通知 Gate 存储元信息
	std::tuple<uint64_t, std::string, uint64_t> UpdatePlayerSessionSnapshot(entt::entity playerEntity,
    uint64_t newSessionId,
    const std::string& newLoginToken,
    uint64_t tokenExpiryMs /* = 0 */)
	{
		auto& registry = tlsRegistryManager.actorRegistry;

		// 如果存在延迟清理计时器，说明之前断线并计划清理，
		// 在我们要更新 snapshot（即认为玩家已成功上线）时应取消该计时器。
		CancelDelayedCleanupTimer(playerEntity);

		auto& sessionPB = registry.get_or_emplace<PlayerSessionSnapshotPBComp>(playerEntity);

		uint64_t oldSessionId = sessionPB.gate_session_id();
		// 旧的 token_id（如果之前存的是 login_token，会返回空串或旧字段）
		std::string oldTokenId;
		if (sessionPB.token_id().size() > 0) {
			oldTokenId = sessionPB.token_id();
		} else {
			// 兼容旧字段：如果历史上只存了 login_token，hash 一下作为旧 id（可选）
			if (sessionPB.login_token().size() > 0) {
				oldTokenId = Sha256Hex(sessionPB.login_token());
			}
		}
		uint64_t oldVersion = sessionPB.session_version();

		// 递增版本（第一次为 1）
		uint64_t newVersion = oldVersion + 1;
		sessionPB.set_gate_session_id(newSessionId);

		// 不再保存明文 token；保存 token_id（hash）与 expiry（0 表示未知/不过期）
		const std::string newTokenId = Sha256Hex(newLoginToken);
		sessionPB.set_token_id(newTokenId);
		sessionPB.set_token_expiry_ms(tokenExpiryMs);

		sessionPB.set_session_version(newVersion);

		// 持久化 snapshot（异步）
		GetPlayerCentreDataRedis()->UpdateExtraData(tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity), sessionPB);

		// 通知 Gate：绑定 session（Centre->Gate 的 BindSession RPC）
		BindSessionToGateRequest bindReq;
		bindReq.set_session_id(newSessionId);
		bindReq.set_player_id(tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(playerEntity));
		bindReq.set_session_version(newVersion);
		SendMessageToGateById(GateBindSessionToGateMessageId, bindReq, GetGateNodeId(newSessionId));

		return { oldSessionId, oldTokenId, oldVersion };
	}

	// 修改：HandleFirstLogin 改为存 token_id 与 expiry（expiry 可为 0）
	void HandleFirstLogin(Guid playerGuid, uint64_t sessionId, const std::string& loginToken, uint64_t tokenExpiryMs = 0)
	{
		PlayerSessionSnapshotPBComp sessionPB;
		sessionPB.set_gate_session_id(sessionId);
		// 不保存明文 token，保存 token_id（hash）
		sessionPB.set_token_id(Sha256Hex(loginToken));
		sessionPB.set_token_expiry_ms(tokenExpiryMs);
		GetPlayerCentreDataRedis()->AsyncLoad(playerGuid, sessionPB);
		GetPlayerCentreDataRedis()->UpdateExtraData(playerGuid, sessionPB);
		LOG_INFO << "HandleFirstLogin for player " << playerGuid;
	}

	// 已有 player 的后续执行（设置进入类型并触发进入场景流程）
	void ApplyEnterGameDecision(entt::entity playerEntity, EnterGameDecision decision)
	{
		auto& registry = tlsRegistryManager.actorRegistry;
		auto& enterComp = registry.get_or_emplace<PlayerEnterGameStatePbComp>(playerEntity);
		switch (decision) {
		case EnterGameDecision::FirstLogin:
			enterComp.set_enter_gs_type(LOGIN_FIRST);
			break;
		case EnterGameDecision::Reconnect:
			enterComp.set_enter_gs_type(LOGIN_RECONNECT);
			break;
		case EnterGameDecision::ReplaceLogin:
			enterComp.set_enter_gs_type(LOGIN_REPLACE);
			break;
		}
		PlayerLifecycleSystem::AddGameNodePlayerToGateNode(playerEntity);
		PlayerLifecycleSystem::ProcessPlayerSessionState(playerEntity);
	}

} // namespace

///<<< END WRITING YOUR CODE


void CentreHandler::GatePlayerService(::google::protobuf::RpcController* controller, const ::GateClientMessageRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}



void CentreHandler::GateSessionDisconnect(::google::protobuf::RpcController* controller, const ::GateSessionDisconnectRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
		//断开链接必须是当前的gate去断，防止异步消息顺序,进入先到然后断开才到
		//考虑a 断 b 断 a 断 b 断.....(中间不断重连)
		//notice 异步过程 gate 先重连过来，然后断开才收到，也就是会把新来的连接又断了，极端情况，也要测试如果这段代码过去了，会有什么问题
		//玩家已经断开连接了

		// Ensure disconnection is handled by the current gate to prevent async message order issues
		// Check for scenarios where reconnect-disconnect cycles might occur in rapid succession
		// Notice: Asynchronous process: If the gate reconnects first and then disconnects, it might
		// inadvertently disconnect a newly arrived connection. Extreme cases need testing to see
	//•	当网络/消息乱序时（Gate 先发 Disconnect，再 Login 的 EnterGame 到达 Centre），会产生竞态：Centre 可能错误删除映射或 Login 再插入时覆盖不一致 snapshot，导致误判、重复清理或无法正确重连；
	/*•	不要在收到 GateSessionDisconnect 时立即从 GlobalSessionList 删除映射；改为：
•	在 GateSessionDisconnect 标记断线（记录 disconnectInfo + snapshot_version）并启动延迟清理；
•	只在延迟清理确定“超时且未重连”时再删除 GlobalSessionList 显射并执行 PlayerLifecycleSystem::HandleNormalExit（已实现的延时回调应负责）。
•	保持 GlobalSessionList 的插入（LoginNodeEnterGame）仍可立即写，用于路由；删除统一由延时清理负责，避免 race。*/

	const uint64_t session_id = request->session_info().session_id();

	const auto player_id = GetPlayerIDBySessionId(session_id);
	if (player_id == kInvalidGuid) return;

	auto playerIt = tlsPlayerList.find(player_id);
	if (playerIt == tlsPlayerList.end()) return;
	auto playerEntity = playerIt->second;

	auto* sessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	if (!sessionPB) return;

	// 如果不是 snapshot 记录的当前 session，忽略（可能是旧断开）
	if (sessionPB->gate_session_id() != session_id) {
		LOG_INFO << "GateSessionDisconnect ignored: mismatch session id for player " << player_id;
		return;
	}

	// 标记离线并记录版本（便于延迟清理时再次校验）
	auto& enterComp = tlsRegistryManager.actorRegistry.get_or_emplace<PlayerEnterGameStatePbComp>(playerEntity);
	enterComp.set_enter_gs_type(LOGIN_DISCONNECTED);

	auto& disconnectInfo = tlsRegistryManager.actorRegistry.get_or_emplace<PlayerDisconnectInfoComp>(playerEntity);
	disconnectInfo.set_disconnected_at(TimeSystem::NowMilliseconds());
	disconnectInfo.set_snapshot_version(sessionPB->session_version());

	// 启动延迟清理（例如 30s）
	StartDelayedCleanupTimer(playerEntity, 30000);

	///<<< END WRITING YOUR CODE
}



void CentreHandler::LoginNodeAccountLogin(::google::protobuf::RpcController* controller, const ::CentreLoginRequest* request,
	::CentreLoginResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	if (tlsPlayerList.size() >= kMaxPlayerSize)
	{
		//如果登录的是新账号,满了得去排队,是帳號排隊，還是角色排隊>???
		response->mutable_error_message()->set_id(kLoginAccountPlayerFull);
		return;
	}
	//排队
	//todo 排队队列里面有同一個人的兩個鏈接
	//如果不是同一個登錄服務器,踢掉已經登錄的賬號
	//告訴客戶端登錄中
///<<< END WRITING YOUR CODE
}



void CentreHandler::LoginNodeEnterGame(::google::protobuf::RpcController* controller, const ::CentrePlayerGameNodeEntryRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
// 替换 LoginNodeEnterGame 内处理登录进入的核心逻辑为：
// 1. 先读取旧 snapshot（如果有），并基于旧值做 DecideEnterGame
// 2. 根据决策有选择地更新 snapshot（递增 version）并在必要时发送 kick
// 3. 执行进入场景副作用
	//•	当网络 / 消息乱序时（Gate 先发 Disconnect，再 Login 的 EnterGame 到达 Centre），会产生竞态：Centre 可能错误删除映射或 Login 再插入时覆盖不一致 snapshot，导致误判、重复清理或无法正确重连；
	/*•	Centre 不必保存 refresh token；access token 在 Login 服务签发并短期有效，Centre 只用于判断重连合法性。
•	为安全起见，记录 token id 或 token 哈希到日志/持久化而不是原文 token。
•	当 token 有 expiry，DecideEnterGame 里使用 oldTokenValid 防止用已过期 token 判定为 Reconnec*/
	const auto& clientInfo = request->client_info();
	const auto& sessionInfo = request->session_info();
	Guid playerId = clientInfo.player_id();
	uint64_t sessionId = sessionInfo.session_id();
	auto& loginToken = request->login_token();

	LOG_INFO << "LoginNodeEnterGame request: player=" << playerId << " session=" << sessionId;

	// 快速路由表更新（单线程模型下直接写）
	GlobalSessionList()[sessionId] = playerId;

	// 是否在内存中已有 player 对象
	auto it = tlsPlayerList.find(playerId);
	if (it == tlsPlayerList.end()) {
		// 首次登录：异步加载/创建玩家数据
		HandleFirstLogin(playerId, sessionId, loginToken);
		return;
	}

	entt::entity playerEntity = it->second;

	// 若客户端/登陆服务同时提供 token_expiry_ms 在 clientMsg，请读取；否则传 0（表示未知/不过期）
	uint64_t incomingTokenExpiryMs = request->token_expiry_ms();

	// 读取旧 snapshot（只读，决策基于旧数据）
	auto* oldSessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(playerEntity);
	const bool hasOldSession = (oldSessionPB != nullptr);
	uint64_t oldSessionId = hasOldSession ? oldSessionPB->gate_session_id() : kInvalidSessionId;
	std::string oldTokenId = hasOldSession ? oldSessionPB->token_id() : std::string{};
	uint64_t oldVersion = hasOldSession ? oldSessionPB->session_version() : 0;

	// 计算 oldTokenValid：如果 proto 中 token_expiry_ms == 0 表示不过期/未知
	uint64_t now_ms = TimeSystem::NowMilliseconds();
	bool oldTokenValid = false;
	if (oldSessionPB) {
	    uint64_t expiry = oldSessionPB->token_expiry_ms();
	    oldTokenValid = (expiry == 0 || expiry > now_ms);
	}

	// 计算 incoming token id
	std::string incomingTokenId = Sha256Hex(loginToken);

	// 决策：只有旧 token 未过期且 token_id 相等才判为重连
	EnterGameDecision decision = DecideEnterGame(hasOldSession, oldTokenId, incomingTokenId, oldTokenValid);

	// 根据决策决定是否更新 snapshot 并是否踢人
	if (decision == EnterGameDecision::ReplaceLogin) {
		auto updated = UpdatePlayerSessionSnapshot(playerEntity, sessionId, loginToken, incomingTokenExpiryMs);
		if (oldSessionId != kInvalidSessionId && oldSessionId != sessionId) {
			SendKickForOldSession(oldSessionId, oldVersion);
			GlobalSessionList().erase(oldSessionId);
		}
	}
	else if (decision == EnterGameDecision::Reconnect) {
		// 重连（不再依赖 session_id 相等判幂等）
		// 1) 取消可能存在的延迟清理定时器（短重连取消清理）
		CancelDelayedCleanupTimer(playerEntity);

		// 2) 始终将 snapshot 绑定到当前 session（递增版本）
		auto updated = UpdatePlayerSessionSnapshot(playerEntity, sessionId, loginToken, incomingTokenExpiryMs);

		// 3) 如果存在不同的旧 session id，清理全局映射并请求 Gate 断开旧会话
		if (oldSessionId != kInvalidSessionId && oldSessionId != sessionId) {
			GlobalSessionList().erase(oldSessionId);
			SendKickForOldSession(oldSessionId, oldVersion);
		}
	}
	else {
		// FirstLogin 已在上层处理
	}

	// 执行进入场景相关副作用（保持原有流程）
	ApplyEnterGameDecision(playerEntity, decision);
	///<<< END WRITING YOUR CODE
}



void CentreHandler::LoginNodeLeaveGame(::google::protobuf::RpcController* controller, const ::LoginNodeLeaveGameRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	if (GlobalSessionList().find(request->session_info().session_id()) == GlobalSessionList().end()) {
		return;
	}

	defer(GlobalSessionList().erase(request->session_info().session_id()));
	const auto player_id = GetPlayerIDBySessionId(request->session_info().session_id());
	PlayerLifecycleSystem::HandleNormalExit(player_id);
	//todo statistics
///<<< END WRITING YOUR CODE
}



void CentreHandler::LoginNodeSessionDisconnect(::google::protobuf::RpcController* controller, const ::GateSessionDisconnectRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	if (GlobalSessionList().find(request->session_info().session_id()) == GlobalSessionList().end()) {
		return;
	}

	defer(GlobalSessionList().erase(request->session_info().session_id()));
	const auto player_id = GetPlayerIDBySessionId(request->session_info().session_id());
	PlayerLifecycleSystem::HandleNormalExit(player_id);
	///<<< END WRITING YOUR CODE
}



void CentreHandler::PlayerService(::google::protobuf::RpcController* controller, const ::NodeRouteMessageRequest* request,
	::NodeRouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	const auto it = GlobalSessionList().find(request->header().session_id());
	if (it == GlobalSessionList().end())
	{
		if (request->message_content().message_id() != CentrePlayerSceneLeaveSceneAsyncSavePlayerCompleteMessageId)
		{
			LOG_ERROR << "Session not found: " << request->header().session_id() << " message id :" << request->message_content().message_id();
			SendErrorToClient(*request, *response, kSessionNotFound);
		}
		return;
	}

	const auto playerId = it->second;
	auto playerEntityIt = tlsPlayerList.find(playerId);
	if (tlsPlayerList.end() == playerEntityIt)
	{
		LOG_ERROR << "Player not found: " << playerId;
		SendErrorToClient(*request, *response, kPlayerNotFoundInSession);
		return;
	}

	const auto player = playerEntityIt->second;
	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "Player not found: " << playerId;
		SendErrorToClient(*request, *response, kPlayerNotFoundInSession);
		return;
	}

	if (request->message_content().message_id() >= gRpcServiceRegistry.size())
	{
		LOG_ERROR << "Message ID not found: " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kMessageIdNotFound);
		return;
	}

	const auto& message_info = gRpcServiceRegistry.at(request->message_content().message_id());

	const auto service_it = gPlayerService.find(message_info.serviceName);
	if (service_it == gPlayerService.end())
	{
		LOG_ERROR << "Player service not found: " << message_info.serviceName;
		return;
	}

	const auto& service_handler = service_it->second;
	google::protobuf::Service* service = service_handler->service();
	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(message_info.methodName);
	if (!method)
	{
		LOG_ERROR << "Method not found: " << message_info.methodName;
		return;
	}

	const MessagePtr playerRequest(service->GetRequestPrototype(method).New());
	if (!playerRequest->ParsePartialFromArray(request->message_content().serialized_message().data(),
		request->message_content().serialized_message().size()))
	{
		LOG_ERROR << "Failed to parse request for message ID: " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kRequestMessageParseError);
		return;
	}

	std::string errorDetails;

	// 检查字段大小
	if (ProtoFieldChecker::CheckFieldSizes(*playerRequest, kProtoFieldCheckerThreshold, errorDetails)) {
		LOG_ERROR << errorDetails << " Failed to check request for message ID: "
			<< request->message_content().message_id();
		SendErrorToClient(*request, *response, kArraySizeTooLargeInMessage);
		return;
	}

	// 检查负数
	if (ProtoFieldChecker::CheckForNegativeInts(*playerRequest, errorDetails)) {
		LOG_ERROR << errorDetails << " Failed to check request for message ID: "
			<< request->message_content().message_id();
		SendErrorToClient(*request, *response, kNegativeValueInMessage);
		return;
	}

	const MessagePtr playerResponse(service->GetResponsePrototype(method).New());

	// Call method on player service handler
	service_handler->CallMethod(method, player, get_pointer(playerRequest), get_pointer(playerResponse));

	// If response is nullptr, no need to send a reply
	if (!response)
	{
		return;
	}

	if (Empty::GetDescriptor() == playerResponse->GetDescriptor()) {
		return;
	}

	response->mutable_header()->set_session_id(request->header().session_id());
	const auto byte_size = playerResponse->ByteSizeLong();
	response->mutable_message_content()->mutable_serialized_message()->resize(byte_size);
	if (!playerResponse->SerializePartialToArray(response->mutable_message_content()->mutable_serialized_message()->data(),
		static_cast<int32_t>(byte_size)))
	{
		LOG_ERROR << "Failed to serialize response for message ID: " << request->message_content().message_id();
		SendErrorToClient(*request, *response, kResponseMessageParseError);
		return;
	}

	response->mutable_message_content()->set_message_id(request->message_content().message_id());

	if (const auto tipInfoMessage = tlsRegistryManager.globalRegistry.try_get<TipInfoMessage>(GlobalEntity());
		nullptr != tipInfoMessage)
	{
		response->mutable_message_content()->mutable_error_message()->CopyFrom(*tipInfoMessage);
		tipInfoMessage->Clear();
	}

	LOG_TRACE << "Successfully processed message ID: " << request->message_content().message_id()
		<< " for player ID: " << playerId;

	///<<< END WRITING YOUR CODE
}



void CentreHandler::EnterGsSucceed(::google::protobuf::RpcController* controller, const ::EnterGameNodeSuccessRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	LOG_TRACE << "Enter Scene Node Succeed request received.";

	const auto playerId = request->player_id();

	auto playerEntityIt = tlsPlayerList.find(playerId);
	if (tlsPlayerList.end() == playerEntityIt)
	{
		LOG_ERROR << "Player not found: " << playerId;
		return;
	}

	const auto player = playerEntityIt->second;
	if (!tlsRegistryManager.actorRegistry.valid(player))
	{
		LOG_ERROR << "Player not found: " << playerId;
		return;
	}

	auto* sessionPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
	if (!sessionPB)
	{
		LOG_ERROR << "Player session info not found for player: " << playerId;
		return;
	}

	auto& nodeIdMap = *sessionPB->mutable_node_id();
	nodeIdMap[eNodeType::SceneNodeService] = request->scene_node_id();

	PlayerLifecycleSystem::AddGameNodePlayerToGateNode(player);

	PlayerChangeRoomUtil::SetCurrentChangeSceneState(player, ChangeRoomInfoPBComponent::eEnterSucceed);
	PlayerChangeRoomUtil::ProgressSceneChangeState(player);

	LOG_INFO << "Player " << playerId << " successfully entered game node " << request->scene_node_id();

	///<<< END WRITING YOUR CODE
}



void CentreHandler::RouteNodeStringMsg(::google::protobuf::RpcController* controller, const ::RouteMessageRequest* request,
	::RouteMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	// Clean up previous routing information
	defer(tlsMessageContext.SetNextRouteNodeType(UINT32_MAX));
	defer(tlsMessageContext.SetNextRouteNodeId(UINT32_MAX));
	defer(tlsMessageContext.SetCurrentSessionId(kInvalidSessionId));

	// Set current session ID
	tlsMessageContext.SetCurrentSessionId(request->session_id());

	if (request->route_nodes_size() >= kMaxRouteSize)
	{
		LOG_ERROR << "Route message size exceeds maximum limit: " << request->DebugString();
		return;
	}

	if (request->route_nodes().empty())
	{
		LOG_ERROR << "Route data list is empty: " << request->DebugString();
		return;
	}

	auto& route_data = request->route_nodes(request->route_nodes_size() - 1);

	if (route_data.message_id() >= gRpcServiceRegistry.size())
	{
		LOG_ERROR << "Message ID not found: " << route_data.message_id();
		return;
	}

	const auto& messageInfo = gRpcServiceRegistry[route_data.message_id()];

	if (!messageInfo.handlerInstance)
	{
		LOG_ERROR << "Message service implementation not found for message ID: " << route_data.message_id();
		return;
	}

	const auto it = gNodeService.find(messageInfo.serviceName);
	if (it == gNodeService.end())
	{
		LOG_ERROR << "Server service handler not found for message ID: " << route_data.message_id();
		return;
	}

	const auto& service = it->second;

	const google::protobuf::MethodDescriptor* method = service->GetDescriptor()->FindMethodByName(messageInfo.methodName);
	if (!method)
	{
		LOG_ERROR << "Method not found: " << messageInfo.methodName;
		return;
	}

	const std::unique_ptr<google::protobuf::Message> current_node_request(service->GetRequestPrototype(method).New());

	if (!current_node_request->ParsePartialFromArray(request->body().data(),
		static_cast<int32_t>(request->body().size())))
	{
		LOG_ERROR << "Failed to parse request serialized_message: " << request->DebugString();
		return;
	}

	const std::unique_ptr<google::protobuf::Message> current_node_response(service->GetResponsePrototype(method).New());

	service->CallMethod(method, nullptr, get_pointer(current_node_request), get_pointer(current_node_response), nullptr);

	auto* mutable_request = const_cast<::RouteMessageRequest*>(request);

	if (tlsMessageContext.GetNextRouteNodeType() == UINT32_MAX)
	{
		const auto byte_size = static_cast<int32_t>(current_node_response->ByteSizeLong());
		response->mutable_body()->resize(byte_size);
		current_node_response->SerializePartialToArray(response->mutable_body()->data(), byte_size);

		// Copy route data list to response
		for (const auto& request_data_it : request->route_nodes())
		{
			*response->add_route_nodes() = request_data_it;
		}

		response->set_session_id(tlsMessageContext.GetSessionId());
		response->set_id(request->id());
		return;
	}

	// Need to route to the next node
	auto* next_route_data = mutable_request->add_route_nodes();
	next_route_data->CopyFrom(tlsMessageContext.GetRoutingNodeInfo());
	next_route_data->mutable_node_info()->CopyFrom(gNode->GetNodeInfo());
	mutable_request->set_body(tlsMessageContext.RouteMsgBody());
	mutable_request->set_id(request->id());

	switch (tlsMessageContext.GetNextRouteNodeType())
	{
	case GateNodeService:
	{
		entt::entity gate_node_id{ tlsMessageContext.GetNextRouteNodeId() };
		auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
		if (!registry.valid(gate_node_id))
		{
			LOG_ERROR << "Gate node not found: " << tlsMessageContext.GetNextRouteNodeId();
			return;
		}
		const auto sceneNodeSession = registry.try_get<RpcSession>(gate_node_id);
		if (!sceneNodeSession)
		{
			LOG_ERROR << "Gate node not found: " << tlsMessageContext.GetNextRouteNodeId();
			return;
		}
		sceneNodeSession->RouteMessageToNode(GateRouteNodeMessageMessageId, *mutable_request);
		break;
	}
	case SceneNodeService:
	{
		entt::entity game_node_id{ tlsMessageContext.GetNextRouteNodeId() };
		auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);
		if (!registry.valid(game_node_id))
		{
			LOG_ERROR << "Game node not found: " << tlsMessageContext.GetNextRouteNodeId() << ", " << request->DebugString();
			return;
		}
		const auto sceneNodeSession = registry.try_get<RpcSession>(game_node_id);
		if (!sceneNodeSession)
		{
			LOG_ERROR << "Game node not found: " << tlsMessageContext.GetNextRouteNodeId() << ", " << request->DebugString();
			return;
		}
		sceneNodeSession->RouteMessageToNode(SceneRouteNodeStringMsgMessageId, *mutable_request);
		break;
	}
	default:
	{
		LOG_ERROR << "Invalid next route node type: " << request->DebugString() << ", " << tlsMessageContext.GetNextRouteNodeType();
		break;
	}
	}
	///<<< END WRITING YOUR CODE
}



void CentreHandler::RoutePlayerStringMsg(::google::protobuf::RpcController* controller, const ::RoutePlayerMessageRequest* request,
	::RoutePlayerMessageResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}



void CentreHandler::InitSceneNode(::google::protobuf::RpcController* controller, const ::InitSceneNodeRequest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	auto sceneNodeId = entt::entity{ request->node_id() };
	auto& registry = tlsNodeContextManager.GetRegistry(eNodeType::SceneNodeService);

	// Check if the scene node ID is valid
	if (!registry.valid(sceneNodeId))
	{
		LOG_ERROR << "Invalid scene node ID: " << request->node_id();
		return;
	}

	// Search for a matching client connection and register the game node
	AddMainRoomToNodeComponent(registry, sceneNodeId);

	LOG_INFO << "Add Scene node " << request->node_id() << " SceneNodeType : " << eSceneNodeType_Name(request->scene_node_type());

	if (request->scene_node_type() == eSceneNodeType::kMainSceneCrossNode)
	{
		registry.remove<MainRoomNode>(sceneNodeId);
		registry.emplace<CrossMainSceneNode>(sceneNodeId);

	}
	else if (request->scene_node_type() == eSceneNodeType::kRoomNode)
	{
		registry.remove<MainRoomNode>(sceneNodeId);
		registry.emplace<RoomSceneNode>(sceneNodeId);
	}
	else if (request->scene_node_type() == eSceneNodeType::kRoomSceneCrossNode)
	{
		registry.remove<MainRoomNode>(sceneNodeId);
		registry.emplace<CrossRoomSceneNode>(sceneNodeId);
	}
	///<<< END WRITING YOUR CODE
}



void CentreHandler::NodeHandshake(::google::protobuf::RpcController* controller, const ::NodeHandshakeRequest* request,
	::NodeHandshakeResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	gNode->GetNodeRegistrationManager().OnNodeHandshake(*request, *response);
	///<<< END WRITING YOUR CODE
}


