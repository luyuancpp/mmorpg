#include "scene_server_player_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/system/player_scene_system.h"
#include "src/pb/pbc/serviceid/clientplayersceneservice_service_method_id.h"

#include "logic_proto/scene_client_player.pb.h"
///<<< END WRITING YOUR CODE
void ServerPlayerSceneServiceHandler::Controller2GsEnterSceneS2C(entt::entity player,
	const ::EnterSceneS2CRequest* request,
	::EnterScenerS2CResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
	auto try_scene = tls.registry.try_get<SceneEntity>(player);
	if (nullptr == try_scene)
	{
		LOG_ERROR << " player not enter scene ";
		return;
	}
	EnterSeceneS2C message;
	message.mutable_scene_info()->CopyFrom(tls.registry.get<SceneInfo>(try_scene->scene_entity_));
	Send2Player(ClientPlayerSceneService_Id_PushEnterSceneS2C, message, player);
///<<< END WRITING YOUR CODE
}
