#pragma once
#include <sol/sol.hpp>
#include "thread_local/storage_lua.h"
#include "common_proto/login_service.pb.h"

class LoginServiceHandler : public ::LoginService
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
				tls_lua_state["LoginServiceLoginHandler"](
				::google::protobuf::internal::DownCast<const ::LoginC2LRequest*>( request),
				::google::protobuf::internal::DownCast<::LoginC2LResponse*>(response));
			}
			break;
			case 1:
			{
				tls_lua_state["LoginServiceCreatePlayerHandler"](
				::google::protobuf::internal::DownCast<const ::CreatePlayerC2LRequest*>( request),
				::google::protobuf::internal::DownCast<::CreatePlayerC2LResponse*>(response));
			}
			break;
			case 2:
			{
				tls_lua_state["LoginServiceEnterGameHandler"](
				::google::protobuf::internal::DownCast<const ::EnterGameC2LRequest*>( request),
				::google::protobuf::internal::DownCast<::EnterGameC2LResponse*>(response));
			}
			break;
			case 3:
			{
				tls_lua_state["LoginServiceLeaveGameHandler"](
				::google::protobuf::internal::DownCast<const ::LeaveGameC2LRequest*>( request),
				::google::protobuf::internal::DownCast<::Empty*>(response));
			}
			break;
			case 4:
			{
				tls_lua_state["LoginServiceDisconnectHandler"](
				::google::protobuf::internal::DownCast<const ::LoginNodeDisconnectRequest*>( request),
				::google::protobuf::internal::DownCast<::Empty*>(response));
			}
			break;
			default:
				GOOGLE_LOG(FATAL) << "Bad method index; this should never happen."
;			break;
		};
	};
};
