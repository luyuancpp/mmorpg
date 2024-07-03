#pragma once
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
#include "client_player_proto/team_client_player.pb.h"

class ClientPlayerTeamServiceHandler : public ::ClientPlayerTeamService
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
				tls_lua_state["ClientPlayerTeamServiceTeamInfoNotifyHandler"](
				::google::protobuf::internal::DownCast<const ::TeamInfoS2CRequest*>( request),
				::google::protobuf::internal::DownCast<::TeamInfoS2CResponse*>(response));
			}
			break;
			default:
				GOOGLE_LOG(FATAL) << "Bad method index; this should never happen."
;			break;
		};
	};
};
