#include "game_service.pb.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"

///<<< BEGIN WRITING YOUR CODE
#include "src/thread_local/game_thread_local_storage.h"
#include "src/game_logic/scene/scene.h"
#include "src/system/player_common_system.h"
///<<< END WRITING YOUR CODE
void GameService::Disconnect(::google::protobuf::RpcController* controller,
	const ::GameNodeDisconnectRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	//异步加载过程中断开了？
	PlayerCommonSystem::RemovePlayereSession(request->player_id());
	auto it = game_tls.player_list().find(request->player_id());
	if (it == game_tls.player_list().end())
	{
		return;
	}
	LeaveSceneParam lp;
	lp.leaver_ = it->second;
	//ScenesSystem::LeaveScene(lp);
	game_tls.player_list().erase(it);//todo  应该是controller 通知过来
///<<< END WRITING YOUR CODE
}
