#include "player_common_system.h"

#include "src/game_logic/player/player_list.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/gate_node.h"
#include "src/network/message_system.h"
#include "src/network/controller_node.h"
#include "src/network/session.h"
#include "src/pb/pbc/service_method/controller_servicemethod.h"
#include "src/pb/pbc/serviceid/serverplayersceneservice_service_method_id.h"
#include "src/thread_local/game_thread_local_storage.h"

#include "component_proto/player_async_comp.pb.h"
#include "component_proto/player_comp.pb.h"
#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_network_comp.pb.h"
#include "controller_service.pb.h"
#include "logic_proto/scene_server_player.pb.h"



void PlayerCommonSystem::OnAsyncLoadPlayerDb(Guid player_id, player_database& message)
{
    auto async_it = game_tls.async_player_data().find(player_id);
    if (async_it == game_tls.async_player_data().end())
    {
		LOG_INFO << "player disconnect" << player_id;
		return;
    }
    auto ret = game_tls.player_list().emplace(player_id, EntityPtr());
	if (!ret.second)
	{
		LOG_ERROR << "server emplace error" << player_id;
		game_tls.async_player_data().erase(async_it);
		return;
	}
    // on loaded db
    entt::entity player = ret.first->second;
	tls.registry.emplace<Player>(player);
    tls.registry.emplace<Guid>(player, player_id);
    tls.registry.emplace<Vector3>(player, message.pos());
   	
    // on load db complete

    EnterGs(player, tls.registry.get<EnterGsInfo>(async_it->second));
	game_tls.async_player_data().erase(async_it);
}

void PlayerCommonSystem::OnAsyncSavePlayerDb(Guid player_id, player_database& message)
{
	//告诉controller 保存完毕，可以切换场景了
	Gs2ControllerLeaveSceneAsyncSavePlayerCompleteRequest save_complete_message;
	Send2ControllerPlayer(ServerPlayerSceneService_Id_Gs2ControllerLeaveSceneAsyncSavePlayerComplete, save_complete_message, player_id);

	game_tls.player_list().erase(player_id);//存储完毕从gs删除玩家
}

void PlayerCommonSystem::SavePlayer(entt::entity player)
{
	using SaveMessagePtr = PlayerDataRedisSystemPtr::element_type::MessageValuePtr;
	SaveMessagePtr pb = std::make_shared<SaveMessagePtr::element_type>();

	pb->set_player_id(tls.registry.get<Guid>(player));
	pb->mutable_pos()->CopyFrom(tls.registry.get<Vector3>(player));

	game_tls.player_data_redis_system()->Save(pb, tls.registry.get<Guid>(player));
}

//考虑: 没load 完再次进入别的gs
void PlayerCommonSystem::EnterGs(entt::entity player, const EnterGsInfo& enter_info)
{
	auto controller_it = game_tls.controller_node().find(enter_info.controller_node_id());
	if (controller_it == game_tls.controller_node().end())
	{
		LOG_ERROR << "EnterGs controller not found" << enter_info.controller_node_id();
		return;
	}
	tls.registry.emplace_or_replace<ControllerNodePtr>(player, controller_it->second);//todo controller 重新启动以后
	EnterGsSucceedRequest rq;
	rq.set_player_id(tls.registry.get<Guid>(player));
	rq.set_game_node_id(node_id());
	controller_it->second->session_->CallMethod(ControllerServiceEnterGsSucceed, &rq);
	//todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
}

void PlayerCommonSystem::LeaveGs(entt::entity player)
{

}

void PlayerCommonSystem::OnPlayerLogin(entt::entity player, uint32_t enter_gs_type)
{
	//第一次登录
	if (enter_gs_type == LOGIN_FIRST)
	{
	}
	else if (enter_gs_type == LOGIN_REPLACE)//顶号
	{
	}
	else if (enter_gs_type == LOGIN_RECONNET)//重连
	{
	}
}

void PlayerCommonSystem::OnEnterGateSucceed(entt::entity player)
{

}

//todo 检测
void PlayerCommonSystem::RemovePlayereSession(Guid player_id)
{
    auto p_it = game_tls.player_list().find(player_id);
    if (p_it == game_tls.player_list().end())//已经在线，直接进入
    {
        return;
    }
	RemovePlayereSession(p_it->second);
}

void PlayerCommonSystem::RemovePlayereSession(entt::entity player)
{
    auto try_get_session = tls.registry.try_get<GateSession>(player);
    if (nullptr == try_get_session)
    {
        return;
    }
    game_tls.gate_sessions().erase(try_get_session->session_id());
}