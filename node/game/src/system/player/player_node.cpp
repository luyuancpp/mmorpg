#include "player_node.h"

#include "network/gate_session.h"
#include "system/network/message_system.h"
#include "service/centre_scene_server_player_service.h"
#include "service/centre_service_service.h"
#include "thread_local/storage.h"
#include "thread_local/storage_common_logic.h"
#include "thread_local/storage_game.h"
#include "type_alias/player_session.h"
#include "util/defer.h"

#include "proto/common/centre_service.pb.h"
#include "proto/logic/component/player_async_comp.pb.h"
#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"

#include "game_node.h"

void PlayerNodeSystem::HandlePlayerAsyncLoaded(Guid playerId, const player_database& message)
{
	LOG_DEBUG << "Player loaded: " << playerId;

	const auto asyncIt = tlsGame.asyncPlayerList.find(playerId);
	if (asyncIt == tlsGame.asyncPlayerList.end())
	{
		LOG_ERROR << "Async player not found: " << playerId;
		return;
	}

	defer(tlsGame.asyncPlayerList.erase(playerId));

	auto player = tls.registry.create();
	if (const auto [fst, snd] = tlsCommonLogic.GetPlayerList().emplace(playerId, player); !snd)
	{
		LOG_ERROR << "Failed to emplace player: " << playerId;
		return;
	}

	// Populate player data from database message
	tls.registry.emplace<Player>(player);
	tls.registry.emplace<Guid>(player, playerId);
	tls.registry.emplace<Transform>(player, message.transform());
	Velocity v;
	v.set_x(1);
	v.set_y(1);
	v.set_z(1);
	tls.registry.emplace<Velocity>(player, v);
	tls.registry.emplace<ViewRadius>(player).set_radius(10);
	tls.registry.emplace<PlayerNodeInfo>(player).set_centre_node_id(asyncIt->second.centre_node_id());

	// todo onload complete

	// Notify game node about player entering
	EnterGs(player, asyncIt->second);
}


void PlayerNodeSystem::HandlePlayerAsyncSaved(Guid playerId, player_database& message)
{
	//todo session 啥时候删除？
	//告诉Centre 保存完毕，可以切换场景了
	CentreLeaveSceneAsyncSavePlayerCompleteRequest request;
	SendToCentrePlayerById(CentreScenePlayerServiceLeaveSceneAsyncSavePlayerCompleteMsgId,
		request,
		playerId);

	if (tls.registry.any_of<UnregisterPlayer>(tlsCommonLogic.GetPlayer(playerId)))
	{
        //存储完毕之后才删除,有没有更好办法做到先删除session 再存储
        RemovePlayerSession(playerId);
        //todo 会不会有问题
        //存储完毕从gs删除玩家
        DestroyPlayer(playerId);
	}  
}

void PlayerNodeSystem::SavePlayer(entt::entity player)
{
	using SaveMessage = PlayerRedis::element_type::MessageValuePtr;
	SaveMessage pb = std::make_shared<SaveMessage::element_type>();

	pb->set_player_id(tls.registry.get<Guid>(player));
	pb->mutable_transform()->CopyFrom(tls.registry.get<Transform>(player));
	tlsGame.playerRedis->Save(pb, tls.registry.get<Guid>(player));
}

//考虑: 没load 完再次进入别的gs
void PlayerNodeSystem::EnterGs(const entt::entity player, const EnterGsInfo& enter_info)
{
	auto* playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == playerNodeInfo)
	{
		LOG_ERROR << "player node info  not found" << enter_info.centre_node_id();
		playerNodeInfo = &tls.registry.emplace<PlayerNodeInfo>(player);
	}
	playerNodeInfo->set_centre_node_id(enter_info.centre_node_id());
	//todo Centre 重新启动以后
	EnterGameNodeSucceedRequest request;
	request.set_player_id(tls.registry.get<Guid>(player));
	request.set_game_node_id(gGameNode->GetNodeId());
	CallCentreNodeMethod(CentreServiceEnterGsSucceedMsgId, request, enter_info.centre_node_id());
	//todo gs更新了对应的gate之后 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，
	//进入game_node a, 再进入game_node b 两个gs的消息到达客户端消息的顺序不一样,所以说game 还要通知game 还要收到gate 的处理完准备离开game的消息
	//否则两个不同的gs可能离开场景的消息后于进入场景的消息到达客户端
}

void PlayerNodeSystem::LeaveGs(entt::entity player)
{
	//todo
}

void PlayerNodeSystem::OnPlayerLogin(entt::entity player, uint32_t enter_gs_type)
{
	//第一次登录
	if (enter_gs_type == LOGIN_FIRST)
	{
	}
	else if (enter_gs_type == LOGIN_REPLACE)
	{
	}
	else if (enter_gs_type == LOGIN_RECONNECT)//重连
	{
	}
}

void PlayerNodeSystem::OnPlayerRegisteredToGateNode(entt::entity player)
{

}

//todo 检测
void PlayerNodeSystem::RemovePlayerSession(const Guid player_id)
{
	auto playerIt = tlsCommonLogic.GetPlayerList().find(player_id);
	if (playerIt == tlsCommonLogic.GetPlayerList().end())
	{
		return;
	}
	RemovePlayerSession(playerIt->second);
}

void PlayerNodeSystem::RemovePlayerSession(entt::entity player)
{
	auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		return;
	}
	defer(tlsSessions.erase(player_node_info->gate_session_id()));
	player_node_info->set_gate_session_id(kInvalidSessionId);
}

void PlayerNodeSystem::DestroyPlayer(Guid player_id)
{
	defer(tlsCommonLogic.GetPlayerList().erase(player_id));
	Destroy(tls.registry, tlsCommonLogic.GetPlayer(player_id));
}

