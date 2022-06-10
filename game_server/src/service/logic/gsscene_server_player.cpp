#include "gsscene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/gate_node.h"
#include "src/network/message_system.h"
#include "src/network/session.h"
#include "src/network/gate_node.h"
#include "src/system/player_common_system.h"
#include "src/system/player_scene_system.h"

#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceImpl::EnterSceneGs2Ms(entt::entity player,
    const ::Gs2MsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::EnterSceneMs2Gs(entt::entity player,
    const ::Ms2GsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    if (request->enter_gs_type() != LOGIN_NONE )
    {
		auto gate_node_id = node_id(request->session_id());
		auto gate_it = g_gate_nodes->find(gate_node_id);
		if (gate_it == g_gate_nodes->end())//test
		{
            LOG_ERROR << " gate not found " << gate_node_id;
            return;
		}
		auto p_gate = registry.try_get<GateNodePtr>(gate_it->second);
		if (nullptr == p_gate)
		{
			LOG_ERROR << " gate not found " << gate_node_id;
			return;

		}
        g_gate_sessions->emplace(request->session_id(), player);
        registry.emplace_or_replace<GateSession>(player).set_session_id(request->session_id());//登录更新gate
		registry.emplace_or_replace<GateNodeWPtr>(player, *p_gate);
        PlayerCommonSystem::OnPlayerLogin(player, request->enter_gs_type());
    }
    //todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
    PlayerSceneSystem::EnterScene(player, request->scene_info().scene_id());
    
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::LeaveSceneGs2Ms(entt::entity player,
    const ::Gs2MsLeaveSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::LeaveSceneMs2Gs(entt::entity player,
    const ::Ms2GsLeaveSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    if (request->change_gs())//存储完毕以后才能换场景，防止回档
	{
        PlayerCommonSystem::SavePlayer(player);
	}
    else
    {
        PlayerSceneSystem::LeaveScene(player);
    }
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::Gs2MsLeaveSceneAsyncSavePlayerComplete(entt::entity player,
    const ::Gs2MsLeaveSceneAsyncSavePlayerCompleteRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<<rpc end
