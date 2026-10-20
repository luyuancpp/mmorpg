#pragma once
#include "proto/scene/scene_admin.pb.h"

// done 由框架传入且非空:不碰它,handler 返回后框架立即回包;要等下游再回复,在返回前 RpcDoneCallback::Adopt(done)(network/rpc_done_callback.h)。
class SceneSceneHandler : public ::SceneScene
{
public:

	void Test(::google::protobuf::RpcController* controller,  const ::GameSceneTest* request, ::Empty* response, ::google::protobuf::Closure* done) override;

	void GmGracefulShutdown(::google::protobuf::RpcController* controller,  const ::GmGracefulShutdownRequest* request, ::GmGracefulShutdownResponse* response, ::google::protobuf::Closure* done) override;

	void GmDescribeEntity(::google::protobuf::RpcController* controller,  const ::GmDescribeEntityRequest* request, ::GmDescribeEntityResponse* response, ::google::protobuf::Closure* done) override;

};