#include "scene.pb.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"// 大家注意把逻辑写的简洁一点，防止文件过大导致编译过久，和生成文件工具读取文件内存不够,函数尽量短小,最好不要超过100行
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void UpdateCrossMainSceneInfo(::google::protobuf::RpcController* controller,
	const ::UpdateCrossMainSceneInfoRequest* request,
	::UpdateCrossMainSceneInfoResponse* response,
	 ::google::protobuf::Closure* done)override;
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}

