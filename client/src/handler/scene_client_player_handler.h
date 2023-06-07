#pragma once
#include <sol/sol.hpp>
#include "src/game_logic/thread_local/thread_local_storage_lua.h"
#include "client_player_proto/scene_client_player.pb.h"

class ClientPlayerSceneServiceHandler : public ::ClientPlayerSceneService
{
public:
	void CallMethod(const ::google::protobuf::MethodDescriptor* method,
	::google::protobuf::RpcController* controller,
	const ::google::protobuf::Message* request,
	::google::protobuf::Message* response,
	::google::protobuf::Closure* done)override
	{
		 switch(method->index())
		{
			case 0:
			{
				tls_lua_state["ClientPlayerSceneServiceEnterSceneC2S"](
				::google::protobuf::internal::DownCast<const ::EnterSceneC2SRequest*>( request),
				::google::protobuf::internal::DownCast<::EnterSceneC2SResponse*>(response));
			}
			break;
			case 1:
			{
				tls_lua_state["ClientPlayerSceneServicePushEnterSceneS2C"](
				::google::protobuf::internal::DownCast<const ::EnterSceneS2C*>( request),
				::google::protobuf::internal::DownCast<::EnterSceneS2C*>(response));
			}
			break;
			case 2:
			{
				tls_lua_state["ClientPlayerSceneServicePushSceneInfoS2C"](
				::google::protobuf::internal::DownCast<const ::SceneInfoS2C*>( request),
				::google::protobuf::internal::DownCast<::SceneInfoS2C*>(response));
			}
			break;
			default:
				GOOGLE_LOG(FATAL) << "Bad method index; this should never happen."
;			break;
		};
	};
};
