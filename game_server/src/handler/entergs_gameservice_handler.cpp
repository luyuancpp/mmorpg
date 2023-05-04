#include "game_service.pb.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"

///<<< BEGIN WRITING YOUR CODE
#include "src/thread_local/game_thread_local_storage.h"
#include "src/system/player_common_system.h"

#include "component_proto/player_async_comp.pb.h"
///<<< END WRITING YOUR CODE
void GameService::EnterGs(::google::protobuf::RpcController* controller,
	const ::GameNodeEnterGsRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
  //连续顶号进入，还在加载中的话继续加载
	auto player_id = request->player_id();
	PlayerCommonSystem::RemovePlayereSession(player_id);
	auto p_it = game_tls.player_list().find(player_id);
	if (p_it != game_tls.player_list().end())//已经在线，直接进入,判断是需要发送哪些信息
	{
		EnterGsInfo enter_info;
		enter_info.set_controller_node_id(request->controller_node_id());
		PlayerCommonSystem::EnterGs(p_it->second, enter_info);
		return;
	}
	auto rit = game_tls.async_player_data().emplace(player_id, EntityPtr());
	if (!rit.second)
	{
		LOG_ERROR << "EnterGs emplace player not found " << player_id;
		return;
	}
	tls.registry.emplace<EnterGsInfo>(rit.first->second).set_controller_node_id(request->controller_node_id());
	game_tls.player_data_redis_system()->AsyncLoad(player_id);//异步加载过程中断开了，怎么处理？
///<<< END WRITING YOUR CODE
}
