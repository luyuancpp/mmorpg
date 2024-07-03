#pragma once
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
#include "client_player_proto/common_client_player.pb.h"

class ClientPlayerCommonServiceHandler : public ::ClientPlayerCommonService
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
				tls_lua_state["ClientPlayerCommonServicePushTipS2CHandler"](
				::google::protobuf::internal::DownCast<const ::TipS2C*>( request),
				::google::protobuf::internal::DownCast<::TipS2C*>(response));
			}
			break;
			case 1:
			{
				tls_lua_state["ClientPlayerCommonServiceBeKickHandler"](
				::google::protobuf::internal::DownCast<const ::TipS2C*>( request),
				::google::protobuf::internal::DownCast<::TipS2C*>(response));
			}
			break;
			default:
				GOOGLE_LOG(FATAL) << "Bad method index; this should never happen."
;			break;
		};
	};
};
