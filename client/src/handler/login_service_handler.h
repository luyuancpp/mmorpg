#pragma once
#include <sol/sol.hpp>
#include "src/thread_local/thread_local_storage_lua.h"
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
				::google::protobuf::internal::DownCast<const ::LoginRequest*>( request),
				::google::protobuf::internal::DownCast<::LoginResponse*>(response));
			}
			break;
			case 1:
			{
				tls_lua_state["LoginServiceCreatePlayerHandler"](
				::google::protobuf::internal::DownCast<const ::CreatePlayerC2lRequest*>( request),
				::google::protobuf::internal::DownCast<::LoginNodeCreatePlayerResponse*>(response));
			}
			break;
			case 2:
			{
				tls_lua_state["LoginServiceEnterGameHandler"](
				::google::protobuf::internal::DownCast<const ::EnterGameRequest*>( request),
				::google::protobuf::internal::DownCast<::EnterGameResponse*>(response));
			}
			break;
			case 3:
			{
				tls_lua_state["LoginServiceLeaveGameHandler"](
				::google::protobuf::internal::DownCast<const ::LeaveGameC2LRequest*>( request),
				::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
			}
			break;
			case 4:
			{
				tls_lua_state["LoginServiceDisconnectHandler"](
				::google::protobuf::internal::DownCast<const ::LoginNodeDisconnectRequest*>( request),
				::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
			}
			break;
			case 5:
			{
				tls_lua_state["LoginServiceRouteNodeStringMsgHandler"](
				::google::protobuf::internal::DownCast<const ::RouteMsgStringRequest*>( request),
				::google::protobuf::internal::DownCast<::RouteMsgStringResponse*>(response));
			}
			break;
			case 6:
			{
				tls_lua_state["LoginServiceRoutePlayerStringMsgHandler"](
				::google::protobuf::internal::DownCast<const ::RoutePlayerMsgStringRequest*>( request),
				::google::protobuf::internal::DownCast<::RoutePlayerMsgStringResponse*>(response));
			}
			break;
			case 7:
			{
				tls_lua_state["LoginServiceGateConnectHandler"](
				::google::protobuf::internal::DownCast<const ::GateConnectRequest*>( request),
				::google::protobuf::internal::DownCast<::google::protobuf::Empty*>(response));
			}
			break;
			default:
				GOOGLE_LOG(FATAL) << "Bad method index; this should never happen."
;			break;
		};
	};
};
