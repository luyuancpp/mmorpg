#include "game_service.pb.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void GameService::ControllerSend2PlayerViaGs(::google::protobuf::RpcController* controller,
	const ::NodeServiceMessageRequest* request,
	::google::protobuf::Empty* response,
	 ::google::protobuf::Closure* done)
{
///<<< BEGIN WRITING YOUR CODE
	::Send2Player(request->msg().service_method_id(), request->msg(), request->ex().player_id());
///<<< END WRITING YOUR CODE
}
