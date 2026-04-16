
#include "scene_admin_handler.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void SceneSceneHandler::Test(::google::protobuf::RpcController* controller, const ::GameSceneTest* request,
	::Empty* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE
}

void SceneSceneHandler::GmGracefulShutdown(::google::protobuf::RpcController* controller, const ::GmGracefulShutdownRequest* request,
	::GmGracefulShutdownResponse* response,
	::google::protobuf::Closure* done)
{
	///<<< BEGIN WRITING YOUR CODE

	LOG_INFO << "GM graceful shutdown requested by operator=" << request->operator_()
			 << " reason=" << request->reason();

	// 1. Save all online players and trigger exit flow.
	auto view = tlsEcs.actorRegistry.view<Player>();
	uint32_t count = 0;
	for (auto entity : view)
	{
		PlayerLifecycleSystem::HandleExitGameNode(entity);
		++count;
	}

	LOG_INFO << "GM graceful shutdown: saved and exited " << count << " players, scheduling node shutdown.";
	response->set_affected_count(count);

	// 2. Reply to GM before shutting down so the caller gets the response.
	done->Run();

	// 3. Schedule shutdown on next loop iteration (after reply is flushed).
	gNode->Shutdown();
	return;

	///<<< END WRITING YOUR CODE
}
