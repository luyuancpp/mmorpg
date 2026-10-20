#pragma once

// gate 侧「本连接已成功 Login」闸的纯判定(docs/design/client-request-ordering.md §7.2 第 2 条;
// docs/design/data-overwrite-rollback-audit-20261018.md §2.3)。
//
// 背景:login 的登录态原先只按 uint32 session_id 建键。session_id = node_id<<17 | 进程内序号,gate 被 kill -9 后以同一
// node_id 重启、序号从 0 开始,新连接会撞上旧进程同号连接 30min TTL 内仍有效的登录态;不先 Login、直接发 CreatePlayer /
// EnterGame,就能在别人的账号下建角、进别人的角色。主修在 login(登录态改按 gate_instance_id + 会话代次建键);
// 这里是 gate 侧的第二道:CreatePlayer / EnterGame / LeaveGame 发出前,要求本连接已收到过成功的 Login 回包。
// 第二道在滚动升级时不可少:新 gate 与旧 login 并存时,旧 login 仍按 session_id 读登录态,只有 gate 挡得住。
//
// 判定时刻是「发出」而不是「收包」:路由 lane 允许客户端不等回包连发 Login → EnterGame(client-request-ordering.md §4,
// ordering-smoke O7),EnterGame 要等 Login 完成之后才发出,那时 Login 的回包已经处理完。直连模式没有 lane,收包即发出,
// 连发的 EnterGame 在 Login 回包之前到达就会被拒(直连本来就不保序;真实客户端要先从 Login 回包的角色列表拿 player_id)。
//
// 纯函数、零状态,单独成文件是为了在 routing_identity_test 里直接测,不把会话表、连接、RPC 注册链拖进测试工程。

#include <cstdint>

#include "proto/common/base/message.pb.h"
#include "proto/login/login.pb.h"
#include "rpc/service_metadata/login_service_metadata.h"

namespace gate_login_admission
{

// 发出前要求本连接已成功 Login 的方法。Login 本身、RefreshToken(凭 refresh token 自证)、Disconnect(gate 合成,
// 不走客户端入口)不在其中。login 侧的 CreatePlayer / EnterGame 读的就是本连接的登录态;LeaveGame 在 login 侧
// 不读登录态(在游戏里待满 SessionExpireMin 后登录态已过期,读了会误拒正常登出),只能在 gate 这一层要求。
inline bool RequiresSuccessfulLogin(uint32_t messageId)
{
	return messageId == ClientPlayerLoginCreatePlayerMessageId ||
		   messageId == ClientPlayerLoginEnterGameMessageId ||
		   messageId == ClientPlayerLoginLeaveGameMessageId;
}

// LoginResponse 表示成功:没有 error_message,或 error_message.id == 0(login 成功时不填;填了 0 也按成功)。
inline bool IsLoginResponseSuccess(const loginpb::LoginResponse &response)
{
	return !response.has_error_message() || response.error_message().id() == 0;
}

// 一条 Login 回包是否表示成功。两种转发模式的回包形状不同:
//   路由模式:路由服回 MessageContent —— 信封 error_message 非 0(路由服或下游拒绝)即失败,否则解 body;
//   直连模式:生成的 gRPC 客户端回的就是 LoginResponse。
// body 解析失败或其他回包类型一律按失败(fail-closed):宁可让客户端重登,也不能凭一个看不懂的回包放行。
inline bool IsLoginReplySuccess(const ::google::protobuf::Message &reply)
{
	if (reply.GetDescriptor() == MessageContent::descriptor())
	{
		const auto &content = static_cast<const MessageContent &>(reply);
		if (content.has_error_message() && content.error_message().id() != 0)
		{
			return false;
		}
		loginpb::LoginResponse response;
		if (!response.ParseFromString(content.serialized_message()))
		{
			return false;
		}
		return IsLoginResponseSuccess(response);
	}
	if (reply.GetDescriptor() == loginpb::LoginResponse::descriptor())
	{
		return IsLoginResponseSuccess(static_cast<const loginpb::LoginResponse &>(reply));
	}
	return false;
}

} // namespace gate_login_admission
